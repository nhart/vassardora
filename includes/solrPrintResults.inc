<?php
class VassardoraSolrPrimaryResults extends IslandoraSolrResults {

  /**
   * Renders the Solr results as a  grid view.
   *
   * @see IslandoraSolrResults::displayResults()
   *
   * @param array $solr_results
   *   The processed Solr results from
   *   IslandoraSolrQueryProcessor::islandoraSolrResult.
   *
   * @return string
   *   Rendered Solr results.
   */
  public function printResults($solr_results) {
    $solr_results = islandora_solr_prepare_solr_results($solr_results);
    $object_results = $solr_results['response']['objects'];
    $object_results = islandora_solr_prepare_solr_doc($object_results);

    $elements = array();
    $elements['solr_total'] = $solr_results['response']['numFound'];
    $elements['solr_start'] = $solr_results['response']['start'];

	// the object_results now display as a Drupal object with label/class/value
	// so try to sniff this out

	$i = 0;

	foreach($object_results as $object_result) {
		// looking for highlight class

		$english_highlight = $german_highlight = "";
			if (array_key_exists("english_mt",$object_result['solr_doc'])) {
				$english_highlight = $object_result['solr_doc']['english_mt']['value'];
			}
			if (array_key_exists("german_mt",$object_result['solr_doc'])) {
				$german_highlight = $object_result['solr_doc']['german_mt']['value'];
			}
	
		$boolEng = $boolGer = false;
		if (stristr($english_highlight,'islandora-solr-highlight'))
			$boolEng = true;
		if (stristr($german_highlight,'islandora-solr-highlight'))
			$boolGer = true;

		// cases: 
		//	English is highlighted, German is not --> take 50 german char
		//	German is highlighted, English is not --> take 50 english char
		// 	Both have a highlight --> all good

		$length = 400;

		if ($boolEng && !$boolGer) {
			$german_highlight = substr($german_highlight,0,$length) . "...";
		}

		else if (!$boolEng && $boolGer) {
			$english_highlight = substr($english_highlight,0,$length) . "...";
		}

		// TODO
	
		if (array_key_exists("english_mt",$object_result['solr_doc'])) {
			$object_result['solr_doc']['english_mt']['value'] = $english_highlight;
		}	
		if (array_key_exists("german_mt",$object_result['solr_doc'])) {
			$object_result['solr_doc']['german_mt']['value'] = $german_highlight;
		}

		// not sure why not saving back, above lines should have done it
		$object_results[$i] = $object_result; 
		$i++;	
		//print_r($object_result); // DEBUG
	}
	
	//print_r($object_results); // DEBUG

    // Return themed search results.
    return theme('islandora_solr', array('results' => $object_results, 'elements' => $elements));


  } // end FUNCTION

	// handy from php.net
	public function getSnippet($str,$max) {
		if(str_word_count($input,0)>$numWords) {
    			$WordKey = str_word_count($input,1);
    			$PosKey = str_word_count($input,2);
    			reset($PosKey);
    			foreach($WordKey as $key => &$value) {
        			$value=key($PosKey);
        			next($PosKey);
    			}
    			return substr($input,0,$WordKey[$numWords]);
  		}
  		else {
			return $input;
		}	
	} // end FUNCTION getSnippet
} // end CLASS

?>
