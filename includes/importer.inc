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
    );

    return $form;
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
    );
    $key = key($info['object_info']);
    $record['object_info'] = $info['processed_objects'][$key] = $info['object_info'][$key];
    unset($info['object_info'][$key]);
    return (empty($record) ? FALSE : new static($record));
  }

  /**
   * @var bool
   *   Stop infinite recursion...
   */
  protected $dcAttempted = FALSE;

  /**
   * Inherits.
   *
   * Somehow generate MODS.
   *
   * @return string
   *   A string containing XML.
   */
  public function getMODS() {
    if ($this->mods === NULL && $this->source['transform'] != 'No Transform') {
      $xml = $this->getXML();
      // Neither MODS or MARCXML (something we can convert to MODS), and we
      // are to transform to DC...
      if (!$this->dcAttempted && !($this->isMODS($xml) || $this->isMARC($xml))) {
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

    return parent::getMODS();
  }

  /**
   * Inherits.
   *
   * Somehow generate DC.
   */
  public function getDC() {
    if ($this->dc === NULL && $this->source['transform'] != 'No Transform') {
      $this->dc = static::runXSLTransform(array(
        'input' => $this->getXML(),
        'xsl' => $this->source['transform'],
      ));
    }

    return parent::getDC();
  }
}
