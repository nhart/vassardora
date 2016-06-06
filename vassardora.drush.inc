<?php

/**
 * @file
 * Drush hooks for the module.
 */

/**
 * Implements hook_drush_command().
 */
function vassardora_drush_command() {
  $items = array();

  $items['vassardora_strip_doc_type'] = array(
    'description' => t('Strip doc type from hocr datastreams.'),
    'drupal dependencies' => array('vassardora', 'islandora'),
    'boostrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $items;
}

/**
 * Command callback to strip doc type.
 */
function drush_vassardora_strip_doc_type() {
  batch_set(vassardora_strip_doc_type_batch());
  drush_backend_batch_process();
}

/**
 * Constructs a batch used to update things via Drush.
 *
 * @return array
 *   An array describing the batch to be executed.
 */
function vassardora_strip_doc_type_batch() {
  return array(
    'operations' => array(
      array(
        'vassardora_strip_doc_type_batch_operation',
        array(),
      ),
    ),
    'title' => dt('Strip doc type from hocr datastreams...'),
    'init_message' => dt('Preparing to remove doc type from hocr datastreams.'),
    'progress_message' => dt('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
    'error_message' => dt('An error has occurred.'),
    'file' => drupal_get_path('module', 'vassardora') . '/vassardora.drush.inc',
  );
}


/**
 * Constructs and performs removing doc type from hoc batch operation.
 *
 * @param array $context
 *   The context of the Druapl batch.
 */
function vassardora_strip_doc_type_batch_operation(&$context) {
  $sandbox = &$context['sandbox'];
  $update = 10;
  $filter_cd = '';
  $doctype_regex = '/<!DOCTYPE\s+html.*?>\s+/si';
  if (isset($sandbox['cd'])) {
    $filter_cd = "FILTER(?cd > '{$sandbox['cd']}'^^xsd:dateTime)";
  }
  $query_string = <<<EOQ
  PREFIX fm: <info:fedora/fedora-system:def/model#>
  PREFIX fr: <info:fedora/fedora-system:def/relations-external#>
  SELECT ?pid ?cd FROM <#ri>
  WHERE {
    ?pid fm:hasModel <info:fedora/islandora:pageCModel> ;
      <info:fedora/fedora-system:def/model#createdDate> ?cd ;
      <fedora-view:disseminates> [<fedora-view:disseminationType> <info:fedora/*/HOCR>].
      $filter_cd
  }
  ORDER BY ?cd
EOQ;

  $connection = islandora_get_tuque_connection();
  if (!isset($sandbox['offset'])) {
    $query = $connection->repository->ri->countQuery($query_string, 'sparql');
    $sandbox['offset'] = 0;
    $sandbox['total'] = $query;
    if ($sandbox['total'] === 0) {
      return;
    }
  }

  $context['message'] = dt('Processing results @start to @end.', array(
    '@start' => $sandbox['offset'],
    '@end' => min($sandbox['offset'] + $update, $sandbox['total']),
  ));
  $query_string .= "
LIMIT $update
";
  $results = $connection->repository->ri->sparqlQuery($query_string);
  if (empty($results)) {
    $context['finished'] = 1;
  }
  foreach ($results as $result) {
    $object = islandora_object_load($result['pid']['value']);
    $sandbox['cd'] = $result['cd']['value'];
    $dsids = array('HOCR', 'RAW_HOCR', 'ENCODED_OCR');
    foreach ($dsids as $dsid) {
      if (isset($object[$dsid])) {
        $object[$dsid]->content = preg_replace($doctype_regex, '', $object[$dsid]->content);
      }
    }
    $sandbox['offset']++;
  }
  $context['finished'] = $sandbox['offset'] / $sandbox['total'];
}