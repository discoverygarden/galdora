<?php

/**
 * @file
 * Manuscript page batch as defined by Galway.
 */

/**
 * Batch preprocessor.
 */
class GaldoraManuscriptBatch extends IslandoraBookBatch {

  /**
   * {@inheritdoc}
   */
  protected static function getObjectClass() {
    return 'GaldoraManuscriptBatchPageObject';
  }

  /**
   * Preprocessor.
   */
  public function preprocess() {
    module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
    $this->parameters['set_id'] = $this->getSetId();
    // Find all the townlands (manuscripts) objects in the target parish
    // (collection).
    $query = <<<EOQ
SELECT ?obj FROM <#ri> WHERE {
  ?obj <fedora-rels-ext:isMemberOfCollection> <info:fedora/{$this->parameters['parish']}> ;
       <fedora-model:hasModel> <info:fedora/islandora:manuscriptCModel>
}
EOQ;
    $townlands = $this->connection->repository->ri->sparqlQuery($query);
    foreach ($townlands as $townland) {
      $townland_object = islandora_object_load($townland['obj']['value']);
      // Matching scheme is contingent on the OBJ's label of the townland
      // matching a directory of images.
      if ($townland_object) {
        if (!isset($townland_object['OBJ'])) {
          drush_log(dt('Unable to find an OBJ on @pid to use to match, skipping this townland.', array(
            '@pid' => $townland_object->id,
          )), 'error');
        }
        $info = pathinfo(trim($townland_object['OBJ']->label));
        $images = $this->findImages($info['filename']);
        $pages = islandora_paged_content_get_pages($townland_object);
        $sequence = count($pages);
        $namespace = $this->determineNamespace($townland_object);
        foreach ($images as $image) {
          $sequence = $sequence + 1;
          $page = new GaldoraManuscriptBatchPageObject($this->connection, $townland_object->id, $sequence, $image, $this->parameters);
          $page->id = $this->connection->repository->getNextIdentifier($namespace);
          $page->addRelationships();
          $this->addToDatabase($page, array());
        }
      }
    }
  }

  /**
   * Helper to find the directory of images to process.
   *
   * @param string $dir
   *   The base directory to process.
   * @param string $image_mask
   *   The mask used to match directories on.
   * @param int $depth
   *   The depth of the search.
   *
   * @return array
   *   An array of directories to be processed.
   *
   * @note This is an almost direct rip off of file_scan_directory beyond
   * wanting to return directories only, not files.
   *
   * @see file_scan_directory()
   */
  public function findImageDirectory($dir, $image_mask, $depth = 0) {
    $directories = array();
    if (is_dir($dir) && ($handle = opendir($dir))) {
      while (FALSE !== ($filename = readdir($handle))) {
        if (!preg_match('/(\\.\\.?|CVS)$/', $filename) && $filename != '.') {
          $uri = "$dir/$filename";
          $uri = file_stream_wrapper_uri_normalize($uri);
          if (is_dir($uri)) {
            if (!preg_match($image_mask, $uri)) {
              $directories = array_merge($this->findImageDirectory($uri, $image_mask, $depth + 1), $directories);
            }
            else {
              $directories[$uri] = $uri;
            }
          }
        }
      }
      closedir($handle);
    }
    return $directories;
  }

  /**
   * Helper to find associated image objects given a label for the directory.
   *
   * @param string $image_label
   *   The label to find the images from.
   *
   * @return array
   *   An array of images to be processed.
   */
  public function findImages($image_label) {
    $image_directories = $this->findImageDirectory($this->parameters['townlands_directory'], "/$image_label/i");
    // Should only be one match, if there's more than one log it and do nothing.
    $images = array();
    if (count($image_directories) > 1) {
      drush_log(dt('Found more than one match for @label, not processing.', array(
        '@label' => $image_label,
      )));
    }
    else {
      $directory = reset($image_directories);
      $images = file_scan_directory($directory, '/.*\.(tif|tiff|TIF|TIFF)/');
      ksort($images, SORT_STRING);
    }
    return $images;
  }

}

/**
 * Class GaldoraManuscriptBatchPageObject.
 */
class GaldoraManuscriptBatchPageObject extends IslandoraBookPageBatchObject {

  /**
   * Function initialBatchState.
   */
  public function initialBatchState() {
    return ISLANDORA_BATCH_STATE__READY;
  }

  /**
   * Function batchProcess.
   */
  public function batchProcess() {
    $this->getObj();
    $this->label = $this->baseName;
    return ISLANDORA_BATCH_STATE__DONE;
  }

  /**
   * Adds the OBJ to the manuscript page.
   */
  public function getObj() {
    $obj_datastream = $this->constructDatastream('OBJ');
    // Figure out the mimetype.
    $obj_datastream->mimetype = static::getMime($this->objectInfo);
    $obj_datastream->label = "OBJ Datastream";
    $obj_datastream->setContentFromFile($this->objectInfo->uri, FALSE);

    $this->ingestDatastream($obj_datastream);
  }

  /**
   * Function addRelationships.
   */
  public function addRelationships() {
    parent::addRelationships();
    $this->models = 'islandora:manuscriptPageCModel';
  }

}
