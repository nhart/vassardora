<?php

/**
 * @file
 * Defines the ZipBatchImporter.
 */

/**
 * Zip file importer.
 * @author adam
 */
class VassardoraZipBatchImporter extends ZipBatchImporter {

  protected $itemClass = 'VassardoraZipBatchImportObject';

  /**
   * Get the form for this importer.
   *
   * @see IslandoraBatchImporter::getForm()
   */
  public static function getForm(array &$form_state) {
    module_load_include('inc', 'xml_form_builder', 'includes/associations');
    // Get the list of valid content models.
    $parent = islandora_object_load($form_state['storage']['parent_pid']);

    $cm_options = array();
    $ns_options = array(
      NULL => t('Default namespace'),
    );
    if (isset($parent['COLLECTION_POLICY'])) {
      $policy_stream = $parent['COLLECTION_POLICY'];
      $policy = new CollectionPolicy($policy_stream->content);

      $cm_options = array();
      $ns_options = array(NULL => t('Default namespace'));
      foreach ($policy->getContentModels() as $pid => $info) {
        $cm_options[$pid] = array('title' => $info['name']);

        $cm_namespace = $info['namespace'];
        if (!empty($cm_namespace)) {
          $cm_namespace = $info['namespace'];
          $cm_namespace = explode(':', $info['namespace']);
          $namespace = trim($cm_namespace[0]);
          $ns_options[$namespace] = $namespace;
        }
      }
    }

    return array(
      'fs' => array(
        '#type' => 'fieldset',
        '#title' => t('Zip Batch Importer'),
        '#description' => t('Select the file containing the assets and metadata to import. Assets and metadata will be matched together based on the portion of the filename without the extension - e.g. my_file.xml and my_file.pdf would be combined into a single object.'),
        'file' => array(
          '#type' => 'managed_file',
          '#title' => t('Zip file containing files to import'),
          '#upload_validators' => array(
            'file_validate_extensions' => array('zip'),
          ),
        ),
        'fs' => array(
          '#type' => 'fieldset',
          '#title' => t('Content Model'),
          '#description' => t('The content model(s) to assign to the imported objects.'),
          'content_model' => array(
            '#type' => 'tableselect',
            '#header' => array('title' => t('Name')),
            '#multiple' => TRUE,
            '#options' => $cm_options,
          ),
        ),
        'tfs' => array(
          '#type' => 'fieldset',
          '#title' => t('XML Datastream Transform'),
          '#description' => t('The XSLT to use when transforming ingested XML.'),
          'xslt' => array(
            '#type' => 'select',
            '#title' => t('XSL Transform'),
            '#options' => array_flip(xml_form_builder_get_transforms()),
            '#default_value' => 'No Transform',
            '#description' => t("An XSL transform for setting the Fedora object's Dublin Core metadata datastream."),
          ),
          'target_dsid' => array(
            '#type' => 'textfield',
            '#description' => t('Datastream ID for transformed XML'),
          ),
        ),
        'namespace' => array(
          '#type' => 'select',
          '#title' => t('Object Namespace'),
          '#description' => t('The namespace in which the imported objects will be created.'),
          '#options' => $ns_options,
        ),
        'submit' => array(
          '#type' => 'submit',
          '#value' => t('Import'),
        ),
      ),
    );
  }

  /**
   * Get the required info to pass into the file parameter of the constructor.
   *
   * @see IslandoraBatchImporter::getBatchInfo()
   */
  public static function getBatchInfo(array &$form_state) {
    // Load the file.
    $file = file_load($form_state['values']['file']);

    // Grab the content models.
    // Cast to an array, in case there are none?
    $content_models = array_filter((array) $form_state['values']['content_model']);

    $info = array(
      'file' => $file,
      'pid_namespace' => $form_state['values']['namespace'],
      'content_model' => $content_models,
      'object_info' => static::getIngestInfo($file),
      'processed_objects' => array(),
      'transform' => $form_state['values']['xslt'],
      'target_dsid' => $form_state['values']['target_dsid'],
    );
    return $info;
  }

}

/**
 * ZIP import object.
 *
 * Actually does the heavy-lifting during the import.
 */
class VassardoraZipBatchImportObject extends ZipBatchImportObject {

  protected $mods;
  protected static $MARC2MODS;
  protected static $DC2MODS;

  /**
   * Constructor.
   */
  protected function __construct($source) {
    parent::__construct($source);
    $this->pidNamespace = $this->source['pid_namespace'];
    $this->contentModel = (array) $this->source['content_model'];
    $transform_path = drupal_get_path('module', 'zip_importer') . '/xsl';
    self::$MARC2MODS = "$transform_path/MARC21slim2MODS3-4.xsl";
    self::$DC2MODS = "$transform_path/simpleDC2MODS.xsl";
    if (strtoupper($source['target_dsid']) == 'MODS') {
      self::$DC2MODS = $source['transform'];
    }
  }

  /**
   * Get an item from the source.
   *
   * @see IslandoraImportObject::getOne()
   */
  public static function getOne(&$info) {
    $record = array(
      'pid_namespace' => $info['pid_namespace'],
      'file' => $info['file'],
      'content_model' => $info['content_model'],
      'transform' => $info['transform'],
      'target_dsid' => $info['target_dsid'],
    );
    $key = key($info['object_info']);
    $record['object_info'] = $info['processed_objects'][$key] = $info['object_info'][$key];
    unset($info['object_info'][$key]);
    return (empty($record) ? FALSE : new static($record));
  }

  /**
   * Determine the datastream ID for the given file.
   *
   * Gets info from the DS-COMPOSITE-MODELs for the selected content models
   * and attempts to match based on detected MIME-Types.
   *
   * @param string $name
   *   A filename for which to determine a mimetype.
   *
   * @return array
   *   An array containing two values:
   *   - Either boolean FALSE or the datastream ID, as mapped in the
   *     DS-COMPOSITE-MODELs of the selected object.
   *   - A string containing the detected mimetype of the file.
   */
  protected function determineDSIDAndMimetype($name) {
    module_load_include('inc', 'islandora', 'includes/utilities');
    // Something like this would be nice to have...  Need a way to indicate
    // primary assets, though...  Think about basic image, where the content
    // model can contain "OBJ" image/jpeg, "TN" image/jpeg and so on...
    static $mime_detect = NULL;
    static $dsids = NULL;
    $models = (array) $this->source['content_model'];

    if ($mime_detect === NULL || $dsids === NULL) {
      $mime_detect = new MimeDetect();
      $dsids = islandora_get_datastreams_requirements_from_models($models);
    }

    $mimetype = $mime_detect->getMimetype($name);
    $dsid = FALSE;
    // Determine which stream this should be, Gather together all
    // non optional datastream ids.
    foreach ($dsids as $ds => $info) {
      if (in_array($mimetype, $info['mime'])) {
        // Remove optional DS by default.
        if (!$info['optional']) {
          $dsid[$ds] = $info;
        }
      }
    }
    // Fire off to the derivative hook to get the primary DSID's.
    $hook_output = islandora_invoke_hook_list(ISLANDORA_DERVIATIVE_CREATION_HOOK, $models, array());
    $source_dsid = FALSE;
    foreach ($dsid as $arr_key => $key_val) {
      foreach ($hook_output as $hook_key => $hook_value) {
        if (isset($hook_output[$hook_key]['source_dsid']) && $hook_output[$hook_key]['source_dsid'] == $dsid[$arr_key]['id']) {
          // We have a match on source object, this is it.
          $source_dsid = $hook_output[$hook_key]['source_dsid'];
        }
      }
    }
    return array($source_dsid, $mimetype);
  }

  /**
   * Acquire base datastreams structure.
   *
   * @see IslandoraImportObject::getDatastreams()
   */
  protected function getDatastreams(&$errors = NULL, &$files = NULL) {
    module_load_include('inc', 'islandora', 'includes/utilities');
    $to_return = parent::getDatastreams($errors, $files);

    foreach ($this->source['object_info'] as $datastream => $name) {
      // Determine which stream this should be...  Uses the first matching, as
      // as received from the DC-COMPOSITE.
      list($dsid, $mimetype) = $this->determineDSIDAndMimetype($name);

      if (!$dsid) {
        $errors[] = array(
          t('The detected mimetype of %filename (@mime) is not supported by any of the selected content models.'),
          array(
            '@mime' => $mimetype,
            '%filename' => $name,
          ),
          WATCHDOG_ERROR,
        );
        continue;
      }

      $zip = new ZipArchive();
      $zip->open(drupal_realpath($this->source['file']->uri));
      if ($zip->extractTo(drupal_realpath('public://'), $name)) {
        $file = new stdClass();
        $file->uri = "public://$name";
        $files[] = $file;

        $to_return += array(
          $dsid => array(
            'dsid' => $dsid,
            'label' => "$dsid datastream",
            'mimetype' => $mimetype,
            'datastream_file' => file_create_url($file->uri),
            'filename' => $name,
            'control_group' => 'M',
            'file' => $file,
          ),
        );
      }
      $zip->close();
    }

    // Correct the labels of the datastreams.
    foreach ($to_return as &$datastream) {
      $datastream['label'] = $this->getLabel($datastream['dsid'], $datastream);
    }
    $this->buildTransformedXml($to_return);
    return $to_return;
  }

  /**
   * Create DC Stream.
   *
   *
   * @return string
   *   A string containing DC XML.
   */
  public function getDC() {
    if ($this->dc === NULL) {
      $xsl = $this->source['target_dsid'] == 'DC' ? $this->source['transform'] : drupal_get_path('module', 'islandora_importer') . '/xsl/mods_to_dc.xsl';
      $input = $this->source['target_dsid'] == 'DC' ? $this->getMODS() : $this->getXML();
      $new_dc = static::runXSLTransform(array(
            'xsl' => $xsl,
            'input' => $input,
      ));
      if ($new_dc) {
        $this->dc = $new_dc;
      }
    }
    return $this->dc;
  }

  /**
   * Transforms XML as required and add to datastream array.
   */
  protected function buildTransformedXml(&$to_return) {
    // DC and MODS have already been altered.
    if (in_array($this->source['target_dsid'], array('DC', 'MODS'))) {
      return;
    }
    foreach ($to_return as $datastream) {
      if ($datastream['mimetype'] == 'text/xml') {
        $input_xml = $datastream['datastream_file'];
        break;
      }
    }
    $new_datstream = static::runXSLTransform(array(
          'xsl' => $this->source['transform'],
          'input' => file_get_contents($input_xml),
    ));

    if ($new_datstream) {
      $file_uri = file_create_filename('data', 'temporary://');
      file_save_data($new_datstream, $file_uri);
      $to_return[] = array(
        'dsid' => $this->source['target_dsid'],
        'label' => $this->source['target_dsid'],
        'mimetype' => 'text/xml',
        'control_group' => 'M',
        'datastream_file' => $file_uri,
      );
    }
  }
}
