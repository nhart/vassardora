<?php

/**
 * @file
 * Drush script for vassardora.
 *
 */

/**
 * Implementation of hook_drush_command().
 */
function vassardora_drush_command() {
  $items = array();
  $items['vbookscope'] = array(
    'callback' => 'vbookscope',
    'description' => 'vassardora check book content',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $items;
}

function vbookscope() {
  $path = drupal_get_path('module', 'vassardora');
  drupal_static_reset('islandora_get_tuque_connection');
  $connection = islandora_get_tuque_connection();
  drush_print($connection->api->a->describeRepository());
  //$query = 'select $object from <#ri> where {$object <fedora-model:hasModel> <info:fedora/islandora:iaBookCModel>}';

  //$objects = $connection->repository->ri->sparqlQuery($query);

  //drush_print("total books to move" . count($objects));
}