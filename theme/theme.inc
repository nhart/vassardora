<?php

/**
 * @file
 *   Various theme hook implementations.
 */

/**
 * Filter and classify the sections of page entries.
 *
 * @param array $pages
 *   The array of pages, as used in the related preprocess functions.
 *
 * @see template_preprocess_vassadora_one_page_fulltext()
 * @see template_preprocess_vassadora_two_page_fulltext()
 */
function vassadora_filter_and_classify_page_entries(array &$pages) {
  foreach ($pages as &$page) {
    $page['sections'] = array_values(array_filter($page['sections'], function($element) {
      return $element['text'];
    }));

    $num_sections = count($page['sections']);
    if ($num_sections == 0) {
      $page['sections'] = array(
        array(
          'label' => t('Not available'),
          'class' => '',
          'text' => t('Sorry, but there was no full text available.'),
        ),
      );
    }
    else {
      foreach ($page['sections'] as &$section) {
        $section['text'] = filter_xss($section['text']);
      }

      if ($num_sections > 1) {
        foreach ($page['sections'] as $index => &$section) {
          $section['class'] = 'text' . ((($index & 1) == 0)?
            'Left':
            'Right');
        }
      }
      elseif ($num_sections == 1) {
        // Need a class to avoid strict warnings.
        $page['sections'][0]['class'] = '';
      }
    }
  }
}

/**
 * Implements template_preprocess_HOOK().
 */
function template_preprocess_vassadora_one_page_fulltext(array &$vars) {
  $object = islandora_object_load(reset($vars['pids']));

  // Add a single "page" with section for each language.
  $vars['pages']['full_text'] = array(
    'label' => t('Full Text'),
    'sections' => array(
      array(
        'label' => t('English'),
        'text' => (isset($object['TRANSCRIPTION_ENG'])?
          $object['TRANSCRIPTION_ENG']->content:
          FALSE),
      ),
      array(
        'label' => t('German'),
        'text' => (isset($object['TRANSCRIPTION_GER'])?
          $object['TRANSCRIPTION_GER']->content:
          FALSE),
      ),
    ),
  );

  vassadora_filter_and_classify_page_entries($vars['pages']);
}

/**
 * Implements template_preprocess_HOOK().
 */
function template_preprocess_vassadora_two_page_fulltext(array &$vars) {
  // Add two "pages", one with each language.
  list($left_pid, $right_pid) = $vars['pids'];
  $left_object = islandora_object_load($left_pid);
  $right_object = islandora_object_load($right_pid);

  $vars['pages']['english'] = array(
    'label' => t('English'),
    'sections' => array(
      array(
        'label' => t('Left'),
        'class' => 'textLeft',
        'text' => (isset($left_object['TRANSCRIPTION_ENG'])?
          $left_object['TRANSCRIPTION_ENG']->content:
          t('No English transcription exists for this page.')),
      ),
      array(
        'label' => t('Right'),
        'class' => 'textRight',
        'text' => (isset($right_object['TRANSCRIPTION_ENG'])?
          $right_object['TRANSCRIPTION_ENG']->content:
          t('No English transcription exists for this page.')),
      ),
    ),
  );
  $vars['pages']['german'] = array(
    'label' => t('German'),
    'sections' => array(
      array(
        'label' => t('Left'),
        'class' => 'textLeft',
        'text' => (isset($left_object['TRANSCRIPTION_GER'])?
          $left_object['TRANSCRIPTION_GER']->content:
          t('No German transcription exists for this page.')),
      ),
      array(
        'label' => t('Right'),
        'class' => 'textRight',
        'text' => (isset($right_object['TRANSCRIPTION_GER'])?
          $right_object['TRANSCRIPTION_GER']->content:
          t('No German transcription exists for this page.')),
      ),
    ),
  );

  vassadora_filter_and_classify_page_entries($vars['pages']);
}

