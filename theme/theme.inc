<?php

/**
 * @file
 *   Various theme hook implementations.
 */

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
        'class' => 'textLeft',
        'text' => (isset($object['TRANSCRIPTION_ENG'])?
          $object['TRANSCRIPTION_ENG']->content:
          t('No English transcription exists for this page.')),
      ),
      array(
        'label' => t('German'),
        'class' => 'textRight',
        'text' => (isset($object['TRANSCRIPTION_GER'])?
          $object['TRANSCRIPTION_GER']->content:
          t('No German transcription exists for this page.')),
      ),
    ),
  );
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
}
