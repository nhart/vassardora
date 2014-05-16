<?php

/**
 * @file
 * Defines the VassadoraZipBatchImporter.
 */

/**
 * Zip file importer.
 */
class VassardoraZipBatchImporter extends ZipBatchImporter {

  protected $itemClass = 'VassardoraZipBatchImportObject';

  /**
   * Get the form for this importer.
   *
   * @see IslandoraBatchImporter::getForm()
   */
  public static function getForm(array &$form_state) {
    $form = parent::getForm($form_state);

    // Reweight, so we can inject our element(s).
    $weight = 0;
    foreach (element_children($form['fs']) as $id) {
      $form['fs'][$id]['#weight'] = $weight++;
    }

    module_load_include('inc', 'xml_form_builder', 'includes/associations');

    $form['fs']['tfs'] = array(
      '#type' => 'fieldset',
      '#title' => t('XML Datastream Transform'),
      '#description' => t('The XSLT to use when transforming ingested XML.'),
      '#weight' => 1.1,
      'xslt' => array(
        '#type' => 'select',
        '#title' => t('XSL Transform'),
        '#options' => array_flip(xml_form_builder_get_transforms()),
        '#default_value' => 'No Transform',
        '#description' => t("An XSL transform for setting the Fedora object's Dublin Core metadata datastream."),
      ),
      'target_dsid' => array(
        '#title' => t('Datastream ID'),
        '#type' => 'textfield',
        '#description' => t('Datastream ID for transformed XML'),
        '#states' => array(
          'invisible' => array(
            ':input[name="xslt"]' => array('value' => 'No Transform'),
          ),
        ),
      ),
    );

    return $form;
  }

  /**
   * Determine if we're ready to run our batch process.
   *
   * @see IslandoraBatchImporter::readyForBatch()
   */
  public static function readyForBatch(array &$form_state) {
    $parent_result = parent::readyForBatch($form_state);

    module_load_include('inc', 'islandora', 'includes/utilities');
    $transform_properly_defined = $form_state['values']['xslt'] == 'No Transform' || islandora_is_valid_dsid($form_state['values']['target_dsid']);
    if (!$transform_properly_defined) {
      form_set_error('target_dsid', t('A valid datastream must be given if a transform has been selected.'));
    }

    return $parent_result && $transform_properly_defined;
  }

  /**
   * Get the required info to pass into the file parameter of the constructor.
   *
   * @see IslandoraBatchImporter::getBatchInfo()
   */
  public static function getBatchInfo(array &$form_state) {
    $info = parent::getBatchInfo($form_state);
    $info += array(
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
  /**
   * Get an item from the source.
   *
   * Overridden to inject a couple parameters...
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
   * Acquire base datastreams structure.
   *
   * @see IslandoraImportObject::getDatastreams()
   */
  protected function getDatastreams(&$errors = NULL, &$files = NULL) {
    $to_return = parent::getDatastreams($errors, $files);
    $this->buildTransformedXml($to_return, $files);
    return $to_return;
  }

  /**
   * @var bool
   *   Stop infinite recursion...
   */
  protected $dcAttempted = FALSE;

  /**
   * Inherits.
   *
   * Generate MODS somehow.
   *
   * @return string
   *   A string containing XML.
   */
  public function getMODS() {
    if ($this->mods === NULL && $this->source['transform'] != 'No Transform') {
      if ($this->source['target_dsid'] == 'MODS') {
        $this->mods = static::runXSLTransform(array(
          'input' => $this->getXML(),
          'xsl' => $this->source['transform'],
        ));
      }
      else {
        $xml = $this->getXML();
        // Neither MODS or MARCXML (something we can convert to MODS), and we
        // are to transform to DC...
        if (!$this->dcAttempted && !($this->isMODS($xml) || $this->isMARC($xml)) && $this->source['target_dsid'] == 'DC') {
          // Let's get the transformed DC, and transform it to MODS...
          // ... this could possibly be better handled by making it call
          // $this->getDC() in the parent code, instead of using the input XML.
          // @see https://github.com/Islandora/islandora_importer/blob/967894c594feb11197afcf190ea9e773c384abfd/modules/zip_importer/includes/importer.inc#L350
          $this->dcAttempted = TRUE;
          $dc = $this->getDC();
          if ($dc) {
            $this->mods = static::runXSLTransform(array(
              'input' => $dc,
              'xsl' => self::$DC2MODS,
            ));
          }
        }
      }
    }

    return parent::getMODS();
  }

  /**
   * Inherits.
   *
   * Generate DC somehow.
   */
  public function getDC() {
    if ($this->dc === NULL && $this->source['transform'] != 'No Transform' && $this->source['target_dsid'] == 'DC') {
      $this->dc = static::runXSLTransform(array(
        'input' => $this->getXML(),
        'xsl' => $this->source['transform'],
      ));
    }

    return parent::getDC();
  }

  /**
   * Transforms XML as required and add to datastream array.
   */
  protected function buildTransformedXml(&$to_return, &$files) {
    if ($this->source['transform'] == 'No Transform' || in_array($this->source['target_dsid'], array('DC', 'MODS'))) {
      // Do not run the transform if we do not have one to run... (or it has
      // already been handled previously).
      return;
    }

    $new_datastream = static::runXSLTransform(array(
      'xsl' => $this->source['transform'],
      'input' => $this->getXML(),
    ));

    if ($new_datastream) {
      // Only drop the entry for the target if we're going to be creating a new
      // one.
      $to_return = array_filter($to_return, array($this, 'filterTarget'));

      $file = file_save_data($new_datastream, 'temporary://');
      $file->status = $file->status & ~FILE_STATUS_PERMANENT;
      $file = file_save($file);
      $files[] = $file;

      $to_return[] = array(
        'dsid' => $this->source['target_dsid'],
        'label' => "{$this->source['target_dsid']} Record",
        'mimetype' => 'text/xml',
        'control_group' => 'M',
        'datastream_file' => $file->uri,
      );
    }
  }

  /**
   * Filter function; ensure target is not defined.
   *
   * @param array $datastream_definition
   *   A datastream definition as provided by getDatastreams().
   *
   * @return bool
   *   TRUE if the definition does not represent an existing target datastream;
   *   otherwise, FALSE.
   */
  protected function filterTarget($datastream_definition) {
    return $this->source['target_dsid'] != $datastream_definition['dsid'];
  }
}
