<?php

/**
 * @file
 * Provides the Views' administrative interface.
 */

use Drupal\Component\Utility\NestedArray;
use Drupal\views\ViewExecutable;

/**
 * Converts a form element in the add view wizard to be AJAX-enabled.
 *
 * This function takes a form element and adds AJAX behaviors to it such that
 * changing it triggers another part of the form to update automatically. It
 * also adds a submit button to the form that appears next to the triggering
 * element and that duplicates its functionality for users who do not have
 * JavaScript enabled (the button is automatically hidden for users who do have
 * JavaScript).
 *
 * To use this function, call it directly from your form builder function
 * immediately after you have defined the form element that will serve as the
 * JavaScript trigger. Calling it elsewhere (such as in hook_form_alter()) may
 * mean that the non-JavaScript fallback button does not appear in the correct
 * place in the form.
 *
 * @param $wrapping_element
 *   The element whose child will server as the AJAX trigger. For example, if
 *   $form['some_wrapper']['triggering_element'] represents the element which
 *   will trigger the AJAX behavior, you would pass $form['some_wrapper'] for
 *   this parameter.
 * @param $trigger_key
 *   The key within the wrapping element that identifies which of its children
 *   serves as the AJAX trigger. In the above example, you would pass
 *   'triggering_element' for this parameter.
 * @param $refresh_parents
 *   An array of parent keys that point to the part of the form that will be
 *   refreshed by AJAX. For example, if triggering the AJAX behavior should
 *   cause $form['dynamic_content']['section'] to be refreshed, you would pass
 *   array('dynamic_content', 'section') for this parameter.
 */
function views_ui_add_ajax_trigger(&$wrapping_element, $trigger_key, $refresh_parents) {
  $seen_ids = &drupal_static(__FUNCTION__ . ':seen_ids', array());
  $seen_buttons = &drupal_static(__FUNCTION__ . ':seen_buttons', array());

  // Add the AJAX behavior to the triggering element.
  $triggering_element = &$wrapping_element[$trigger_key];
  $triggering_element['#ajax']['callback'] = 'views_ui_ajax_update_form';
  // We do not use drupal_html_id() to get an ID for the AJAX wrapper, because
  // it remembers IDs across AJAX requests (and won't reuse them), but in our
  // case we need to use the same ID from request to request so that the
  // wrapper can be recognized by the AJAX system and its content can be
  // dynamically updated. So instead, we will keep track of duplicate IDs
  // (within a single request) on our own, later in this function.
  $triggering_element['#ajax']['wrapper'] = 'edit-view-' . implode('-', $refresh_parents) . '-wrapper';

  // Add a submit button for users who do not have JavaScript enabled. It
  // should be displayed next to the triggering element on the form.
  $button_key = $trigger_key . '_trigger_update';
  $wrapping_element[$button_key] = array(
    '#type' => 'submit',
    // Hide this button when JavaScript is enabled.
    '#attributes' => array('class' => array('js-hide')),
    '#submit' => array('views_ui_nojs_submit'),
    // Add a process function to limit this button's validation errors to the
    // triggering element only. We have to do this in #process since until the
    // form API has added the #parents property to the triggering element for
    // us, we don't have any (easy) way to find out where its submitted values
    // will eventually appear in $form_state['values'].
    '#process' => array_merge(array('views_ui_add_limited_validation'), element_info_property('submit', '#process', array())),
    // Add an after-build function that inserts a wrapper around the region of
    // the form that needs to be refreshed by AJAX (so that the AJAX system can
    // detect and dynamically update it). This is done in #after_build because
    // it's a convenient place where we have automatic access to the complete
    // form array, but also to minimize the chance that the HTML we add will
    // get clobbered by code that runs after we have added it.
    '#after_build' => array_merge(element_info_property('submit', '#after_build', array()), array('views_ui_add_ajax_wrapper')),
  );
  // Copy #weight and #access from the triggering element to the button, so
  // that the two elements will be displayed together.
  foreach (array('#weight', '#access') as $property) {
    if (isset($triggering_element[$property])) {
      $wrapping_element[$button_key][$property] = $triggering_element[$property];
    }
  }
  // For easiest integration with the form API and the testing framework, we
  // always give the button a unique #value, rather than playing around with
  // #name.
  $button_title = !empty($triggering_element['#title']) ? $triggering_element['#title'] : $trigger_key;
  if (empty($seen_buttons[$button_title])) {
    $wrapping_element[$button_key]['#value'] = t('Update "@title" choice', array(
      '@title' => $button_title,
    ));
    $seen_buttons[$button_title] = 1;
  }
  else {
    $wrapping_element[$button_key]['#value'] = t('Update "@title" choice (@number)', array(
      '@title' => $button_title,
      '@number' => ++$seen_buttons[$button_title],
    ));
  }

  // Attach custom data to the triggering element and submit button, so we can
  // use it in both the process function and AJAX callback.
  $ajax_data = array(
    'wrapper' => $triggering_element['#ajax']['wrapper'],
    'trigger_key' => $trigger_key,
    'refresh_parents' => $refresh_parents,
    // Keep track of duplicate wrappers so we don't add the same wrapper to the
    // page more than once.
    'duplicate_wrapper' => !empty($seen_ids[$triggering_element['#ajax']['wrapper']]),
  );
  $seen_ids[$triggering_element['#ajax']['wrapper']] = TRUE;
  $triggering_element['#views_ui_ajax_data'] = $ajax_data;
  $wrapping_element[$button_key]['#views_ui_ajax_data'] = $ajax_data;
}

/**
 * Processes a non-JavaScript fallback submit button to limit its validation errors.
 */
function views_ui_add_limited_validation($element, &$form_state) {
  // Retrieve the AJAX triggering element so we can determine its parents. (We
  // know it's at the same level of the complete form array as the submit
  // button, so all we have to do to find it is swap out the submit button's
  // last array parent.)
  $array_parents = $element['#array_parents'];
  array_pop($array_parents);
  $array_parents[] = $element['#views_ui_ajax_data']['trigger_key'];
  $ajax_triggering_element = NestedArray::getValue($form_state['complete_form'], $array_parents);

  // Limit this button's validation to the AJAX triggering element, so it can
  // update the form for that change without requiring that the rest of the
  // form be filled out properly yet.
  $element['#limit_validation_errors'] = array($ajax_triggering_element['#parents']);

  // If we are in the process of a form submission and this is the button that
  // was clicked, the form API workflow in form_builder() will have already
  // copied it to $form_state['triggering_element'] before our #process
  // function is run. So we need to make the same modifications in $form_state
  // as we did to the element itself, to ensure that #limit_validation_errors
  // will actually be set in the correct place.
  if (!empty($form_state['triggering_element'])) {
    $clicked_button = &$form_state['triggering_element'];
    if ($clicked_button['#name'] == $element['#name'] && $clicked_button['#value'] == $element['#value']) {
      $clicked_button['#limit_validation_errors'] = $element['#limit_validation_errors'];
    }
  }

  return $element;
}

/**
 * After-build function that adds a wrapper to a form region (for AJAX refreshes).
 *
 * This function inserts a wrapper around the region of the form that needs to
 * be refreshed by AJAX, based on information stored in the corresponding
 * submit button form element.
 */
function views_ui_add_ajax_wrapper($element, &$form_state) {
  // Don't add the wrapper <div> if the same one was already inserted on this
  // form.
  if (empty($element['#views_ui_ajax_data']['duplicate_wrapper'])) {
    // Find the region of the complete form that needs to be refreshed by AJAX.
    // This was earlier stored in a property on the element.
    $complete_form = &$form_state['complete_form'];
    $refresh_parents = $element['#views_ui_ajax_data']['refresh_parents'];
    $refresh_element = NestedArray::getValue($complete_form, $refresh_parents);

    // The HTML ID that AJAX expects was also stored in a property on the
    // element, so use that information to insert the wrapper <div> here.
    $id = $element['#views_ui_ajax_data']['wrapper'];
    $refresh_element += array(
      '#prefix' => '',
      '#suffix' => '',
    );
    $refresh_element['#prefix'] = '<div id="' . $id . '" class="views-ui-ajax-wrapper">' . $refresh_element['#prefix'];
    $refresh_element['#suffix'] .= '</div>';

    // Copy the element that needs to be refreshed back into the form, with our
    // modifications to it.
    NestedArray::setValue($complete_form, $refresh_parents, $refresh_element);
  }

  return $element;
}

/**
 * Updates a part of the add view form via AJAX.
 *
 * @return
 *   The part of the form that has changed.
 */
function views_ui_ajax_update_form($form, $form_state) {
  // The region that needs to be updated was stored in a property of the
  // triggering element by views_ui_add_ajax_trigger(), so all we have to do is
  // retrieve that here.
  return NestedArray::getValue($form, $form_state['triggering_element']['#views_ui_ajax_data']['refresh_parents']);
}

/**
 * Non-Javascript fallback for updating the add view form.
 */
function views_ui_nojs_submit($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
}

/**
 * Form element validation handler for a taxonomy autocomplete field.
 *
 * This allows a taxonomy autocomplete field to be validated outside the
 * standard Field API workflow, without passing in a complete field widget.
 * Instead, all that is required is that $element['#field_name'] contain the
 * name of the taxonomy autocomplete field that is being validated.
 *
 * This function is currently not used for validation directly, although it
 * could be. Instead, it is only used to store the term IDs and vocabulary name
 * in the element value, based on the tags that the user typed in.
 *
 * @see taxonomy_autocomplete_validate()
 */
function views_ui_taxonomy_autocomplete_validate($element, &$form_state) {
  $value = array();
  if ($tags = $element['#value']) {
    // Get the machine names of the vocabularies we will search, keyed by the
    // vocabulary IDs.
    $field = field_info_field($element['#field_name']);
    $vocabularies = array();
    if (!empty($field['settings']['allowed_values'])) {
      foreach ($field['settings']['allowed_values'] as $tree) {
        if ($vocabulary = entity_load('taxonomy_vocabulary', $tree['vocabulary'])) {
          $vocabularies[$vocabulary->id()] = $tree['vocabulary'];
        }
      }
    }
    // Store the term ID of each (valid) tag that the user typed.
    $typed_terms = drupal_explode_tags($tags);
    foreach ($typed_terms as $typed_term) {
      if ($terms = entity_load_multiple_by_properties('taxonomy_term', array('name' => trim($typed_term), 'vid' => array_keys($vocabularies)))) {
        $term = array_pop($terms);
        $value['tids'][] = $term->tid;
      }
    }
    // Store the term IDs along with the name of the vocabulary. Currently
    // Views (as well as the Field UI) assumes that there will only be one
    // vocabulary, although technically the API allows there to be more than
    // one.
    if (!empty($value['tids'])) {
      $value['tids'] = array_unique($value['tids']);
      $value['vocabulary'] = array_pop($vocabularies);
    }
  }
  form_set_value($element, $value, $form_state);
}

/**
 * Move form elements into details for presentation purposes.
 *
 * Many views forms use #tree = TRUE to keep their values in a hierarchy for
 * easier storage. Moving the form elements into fieldsets during form building
 * would break up that hierarchy. Therefore, we wait until the pre_render stage,
 * where any changes we make affect presentation only and aren't reflected in
 * $form_state['values'].
 */
function views_ui_pre_render_add_fieldset_markup($form) {
  foreach (element_children($form) as $key) {
    $element = $form[$key];
    // In our form builder functions, we added an arbitrary #fieldset property
    // to any element that belongs in a fieldset. If this form element has that
    // property, move it into its fieldset.
    if (isset($element['#fieldset']) && isset($form[$element['#fieldset']])) {
      $form[$element['#fieldset']][$key] = $element;
      // Remove the original element this duplicates.
      unset($form[$key]);
    }
  }

  return $form;
}

/**
 * Flattens the structure of an element containing the #flatten property.
 *
 * If a form element has #flatten = TRUE, then all of it's children
 * get moved to the same level as the element itself.
 * So $form['to_be_flattened'][$key] becomes $form[$key], and
 * $form['to_be_flattened'] gets unset.
 */
function views_ui_pre_render_flatten_data($form) {
  foreach (element_children($form) as $key) {
    $element = $form[$key];
    if (!empty($element['#flatten'])) {
      foreach (element_children($element) as $child_key) {
        $form[$child_key] = $form[$key][$child_key];
      }
      // All done, remove the now-empty parent.
      unset($form[$key]);
    }
  }

  return $form;
}

/**
 * Moves argument options into their place.
 *
 * When configuring the default argument behavior, almost each of the radio
 * buttons has its own fieldset shown bellow it when the radio button is
 * clicked. That fieldset is created through a custom form process callback.
 * Each element that has #argument_option defined and pointing to a default
 * behavior gets moved to the appropriate fieldset.
 * So if #argument_option is specified as 'default', the element is moved
 * to the 'default_options' fieldset.
 */
function views_ui_pre_render_move_argument_options($form) {
  foreach (element_children($form) as $key) {
    $element = $form[$key];
    if (!empty($element['#argument_option'])) {
      $container_name = $element['#argument_option'] . '_options';
      if (isset($form['no_argument']['default_action'][$container_name])) {
        $form['no_argument']['default_action'][$container_name][$key] = $element;
      }
      // Remove the original element this duplicates.
      unset($form[$key]);
    }
  }
  return $form;
}

/**
 * Add a <select> dropdown for a given section, allowing the user to
 * change whether this info is stored on the default display or on
 * the current display.
 */
function views_ui_standard_display_dropdown(&$form, &$form_state, $section) {
  $view = &$form_state['view'];
  $display_id = $form_state['display_id'];
  $executable = $view->get('executable');
  $displays = $executable->displayHandlers;
  $current_display = $executable->display_handler;

  // @todo Move this to a separate function if it's needed on any forms that
  // don't have the display dropdown.
  $form['override'] = array(
    '#prefix' => '<div class="views-override clearfix container-inline">',
    '#suffix' => '</div>',
    '#weight' => -1000,
    '#tree' => TRUE,
  );

  // Add the "2 of 3" progress indicator.
  if ($form_progress = $view->getFormProgress()) {
    $form['progress']['#markup'] = '<div id="views-progress-indicator">' . t('@current of @total', array('@current' => $form_progress['current'], '@total' => $form_progress['total'])) . '</div>';
    $form['progress']['#weight'] = -1001;
  }

  if ($current_display->isDefaultDisplay()) {
    return;
  }

  // Determine whether any other displays have overrides for this section.
  $section_overrides = FALSE;
  $section_defaulted = $current_display->isDefaulted($section);
  foreach ($displays as $id => $display) {
    if ($id === 'default' || $id === $display_id) {
      continue;
    }
    if ($display && !$display->isDefaulted($section)) {
      $section_overrides = TRUE;
    }
  }

  $display_dropdown['default'] = ($section_overrides ? t('All displays (except overridden)') : t('All displays'));
  $display_dropdown[$display_id] = t('This @display_type (override)', array('@display_type' => $current_display->getPluginId()));
  // Only display the revert option if we are in a overridden section.
  if (!$section_defaulted) {
    $display_dropdown['default_revert'] = t('Revert to default');
  }

  $form['override']['dropdown'] = array(
    '#type' => 'select',
    '#title' => t('For'), // @TODO: Translators may need more context than this.
    '#options' => $display_dropdown,
  );
  if ($current_display->isDefaulted($section)) {
    $form['override']['dropdown']['#default_value'] = 'defaults';
  }
  else {
    $form['override']['dropdown']['#default_value'] = $display_id;
  }

}

/**
 * Create the URL for one of our standard AJAX forms based upon known
 * information about the form.
 */
function views_ui_build_form_url($form_state) {
  $ajax = empty($form_state['ajax']) ? 'nojs' : 'ajax';
  $name = $form_state['view']->id();
  $url = "admin/structure/views/$ajax/$form_state[form_key]/$name/$form_state[display_id]";
  if (isset($form_state['type'])) {
    $url .= '/' . $form_state['type'];
  }
  if (isset($form_state['id'])) {
    $url .= '/' . $form_state['id'];
  }
  return $url;
}

function _views_sort_types($a, $b) {
  $a_group = drupal_strtolower($a['group']);
  $b_group = drupal_strtolower($b['group']);
  if ($a_group != $b_group) {
    return $a_group < $b_group ? -1 : 1;
  }

  $a_title = drupal_strtolower($a['title']);
  $b_title = drupal_strtolower($b['title']);
  if ($a_title != $b_title) {
    return $a_title < $b_title ? -1 : 1;
  }

  return 0;
}

/**
 * Fetch a list of all fields available for a given base type.
 *
 * @param (array|string) $base
 *   A list or a single base_table, for example node.
 * @param string $type
 *   The handler type, for example field or filter.
 * @param bool $grouping
 *   Should the result grouping by its 'group' label.
 * @param string $sub_type
 *   An optional sub type. E.g. Allows making an area plugin available for
 *   header only, instead of header, footer, and empty regions.
 *
 * @return array
 *   A keyed array of in the form of 'base_table' => 'Description'.
 */
function views_fetch_fields($base, $type, $grouping = FALSE, $sub_type = NULL) {
  static $fields = array();
  if (empty($fields)) {
    $data = drupal_container()->get('views.views_data')->get();
    $start = microtime(TRUE);
    // This constructs this ginormous multi dimensional array to
    // collect the important data about fields. In the end,
    // the structure looks a bit like this (using nid as an example)
    // $strings['nid']['filter']['title'] = 'string'.
    //
    // This is constructed this way because the above referenced strings
    // can appear in different places in the actual data structure so that
    // the data doesn't have to be repeated a lot. This essentially lets
    // each field have a cheap kind of inheritance.

    foreach ($data as $table => $table_data) {
      $bases = array();
      $strings = array();
      $skip_bases = array();
      foreach ($table_data as $field => $info) {
        // Collect table data from this table
        if ($field == 'table') {
          // calculate what tables this table can join to.
          if (!empty($info['join'])) {
            $bases = array_keys($info['join']);
          }
          // And it obviously joins to itself.
          $bases[] = $table;
          continue;
        }
        foreach (array('field', 'sort', 'filter', 'argument', 'relationship', 'area') as $key) {
          if (!empty($info[$key])) {
            if ($grouping && !empty($info[$key]['no group by'])) {
              continue;
            }
            if ($sub_type && isset($info[$key]['sub_type']) && (!in_array($sub_type, (array) $info[$key]['sub_type']))) {
              continue;
            }
            if (!empty($info[$key]['skip base'])) {
              foreach ((array) $info[$key]['skip base'] as $base_name) {
                $skip_bases[$field][$key][$base_name] = TRUE;
              }
            }
            elseif (!empty($info['skip base'])) {
              foreach ((array) $info['skip base'] as $base_name) {
                $skip_bases[$field][$key][$base_name] = TRUE;
              }
            }
            foreach (array('title', 'group', 'help', 'base', 'aliases') as $string) {
              // First, try the lowest possible level
              if (!empty($info[$key][$string])) {
                $strings[$field][$key][$string] = $info[$key][$string];
              }
              // Then try the field level
              elseif (!empty($info[$string])) {
                $strings[$field][$key][$string] = $info[$string];
              }
              // Finally, try the table level
              elseif (!empty($table_data['table'][$string])) {
                $strings[$field][$key][$string] = $table_data['table'][$string];
              }
              else {
                if ($string != 'base' && $string != 'base') {
                  $strings[$field][$key][$string] = t("Error: missing @component", array('@component' => $string));
                }
              }
            }
          }
        }
      }
      foreach ($bases as $base_name) {
        foreach ($strings as $field => $field_strings) {
          foreach ($field_strings as $type_name => $type_strings) {
            if (empty($skip_bases[$field][$type_name][$base_name])) {
              $fields[$base_name][$type_name]["$table.$field"] = $type_strings;
            }
          }
        }
      }
    }
  }

  // If we have an array of base tables available, go through them
  // all and add them together. Duplicate keys will be lost and that's
  // Just Fine.
  if (is_array($base)) {
    $strings = array();
    foreach ($base as $base_table) {
      if (isset($fields[$base_table][$type])) {
        $strings += $fields[$base_table][$type];
      }
    }
    uasort($strings, '_views_sort_types');
    return $strings;
  }

  if (isset($fields[$base][$type])) {
    uasort($fields[$base][$type], '_views_sort_types');
    return $fields[$base][$type];
  }
  return array();
}

/**
 * #process callback for a button; determines if a button is the form's triggering element.
 *
 * The Form API has logic to determine the form's triggering element based on
 * the data in $_POST. However, it only checks buttons based on a single #value
 * per button. This function may be added to a button's #process callbacks to
 * extend button click detection to support multiple #values per button. If the
 * data in $_POST matches any value in the button's #values array, then the
 * button is detected as having been clicked. This can be used when the value
 * (label) of the same logical button may be different based on context (e.g.,
 * "Apply" vs. "Apply and continue").
 *
 * @see _form_builder_handle_input_element()
 * @see _form_button_was_clicked()
 */
function views_ui_form_button_was_clicked($element, &$form_state) {
  $process_input = empty($element['#disabled']) && ($form_state['programmed'] || ($form_state['process_input'] && (!isset($element['#access']) || $element['#access'])));
  if ($process_input && !isset($form_state['triggering_element']) && !empty($element['#is_button']) && isset($form_state['input'][$element['#name']]) && isset($element['#values']) && in_array($form_state['input'][$element['#name']], $element['#values'], TRUE)) {
    $form_state['triggering_element'] = $element;
  }
  return $element;
}
