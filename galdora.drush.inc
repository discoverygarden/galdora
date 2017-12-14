<?php

/**
 * @file
 * Drush commands for Galway.
 */

/**
 * Implements hook_drush_command().
 */
function galdora_drush_command() {
  $items = array();
  $items['galdora_manuscript_batch_preprocess'] = array(
    'aliases' => array('gmbp'),
    'description' => dt('Preprocesses images into manuscript page children given a manuscript object.'),
    'drupal dependencies' => array(
      'islandora_batch',
      'islandora_book_batch',
      'islandora',
      'galdora',
    ),
    'options' => array(
      'townlands_directory' => array(
        'description' => dt('The path to the "directory" of townland page images.'),
        'required' => TRUE,
      ),
      'parish' => array(
        'description' => dt('The target parish (collection) which contains townlands (manuscript objects and their pages) to be processed.'),
        'required' => TRUE,
      ),
    ),
  );
  return $items;
}

/**
 * Callback that sets up a batch preprocessor for a parish's townlands.
 */
function drush_galdora_manuscript_batch_preprocess() {
  drupal_static_reset('islandora_get_tuque_connection');
  $connection = islandora_get_tuque_connection();

  $parameters = array(
    'townlands_directory' => drush_get_option('townlands_directory'),
    'parish' => drush_get_option('parish'),
    // Set these parameters to handle namespace restrictions.
    'parent' => drush_get_option('parish'),
    'namespace' => '',
  );
  $preprocessor = new GaldoraManuscriptBatch($connection, $parameters);
  islandora_batch_handle_preprocessor($preprocessor);
  drush_print($preprocessor->getSetId());
}

/**
 * Validation for the Galway manuscript batch preprocess.
 */
function drush_galdora_manuscript_batch_preprocess_validate() {
  $images = drush_get_option('townlands_directory');
  if (!is_dir($images)) {
    drush_set_error('galdora', dt('The directory (@directory) of images does not exist.', array(
      '@directory' => $images,
    )));
  }
  $pid = drush_get_option('parish');
  $object = islandora_object_load($pid);
  if ($object) {
    if (!in_array('islandora:collectionCModel', $object->models)) {
      drush_set_error('galdora', dt('The object (@pid) is not a collection object.', array(
        '@pid' => $pid,
      )));
    }
    else {
      drupal_static_reset('islandora_get_tuque_connection');
      $connection = islandora_get_tuque_connection();
      // See if there are any valid children object before continuing.
      $query = <<<EOQ
SELECT ?obj FROM <#ri> WHERE {
  ?obj <fedora-rels-ext:isMemberOfCollection> <info:fedora/$pid> ;
       <fedora-model:hasModel> <info:fedora/islandora:manuscriptCModel>
}
EOQ;
      if ($connection->repository->ri->countQuery($query, 'sparql') == 0) {
        drush_set_error('galdora', dt('No valid children in @pid to preprocess.', array(
          '@pid' => $pid,
        )));
      }
    }
  }
  else {
    drush_set_error('galdora', dt('The object (@pid) does not exist in Fedora.', array(
      '@pid' => $pid,
    )));
  }
}
