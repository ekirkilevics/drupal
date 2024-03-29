<?php

/**
 * @file
 * Contains Drupal\views_ui\ViewEditFormController.
 */

namespace Drupal\views_ui;

use Drupal\Core\Ajax\AjaxResponse;
use Drupal\Core\Ajax\HtmlCommand;
use Drupal\Core\Ajax\ReplaceCommand;
use Drupal\Core\Entity\EntityInterface;
use Drupal\Component\Utility\NestedArray;
use Drupal\views\ViewExecutable;

/**
 * Form controller for the Views edit form.
 */
class ViewEditFormController extends ViewFormControllerBase {

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::getEntity().
   */
  public function getEntity(array $form_state) {
    return isset($form_state['view']) ? $form_state['view'] : NULL;
  }

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::setEntity().
   */
  public function setEntity(EntityInterface $entity, array &$form_state) {
    $form_state['view'] = $entity;
  }

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::form().
   */
  public function form(array $form, array &$form_state, EntityInterface $view) {
    $display_id = $this->displayID;
    // Do not allow the form to be cached, because $form_state['view'] can become
    // stale between page requests.
    // See views_ui_ajax_get_form() for how this affects #ajax.
    // @todo To remove this and allow the form to be cacheable:
    //   - Change $form_state['view'] to $form_state['temporary']['view'].
    //   - Add a #process function to initialize $form_state['temporary']['view']
    //     on cached form submissions.
    //   - Use form_load_include().
    $form_state['no_cache'] = TRUE;

    if ($display_id) {
      if (!$view->get('executable')->setDisplay($display_id)) {
        $form['#markup'] = t('Invalid display id @display', array('@display' => $display_id));
        return $form;
      }
    }

    $form['#tree'] = TRUE;

    $form['#attached']['library'][] = array('system', 'jquery.ui.tabs');
    $form['#attached']['library'][] = array('system', 'jquery.ui.dialog');
    $form['#attached']['library'][] = array('system', 'drupal.states');
    $form['#attached']['library'][] = array('system', 'drupal.tabledrag');

    if (!config('views.settings')->get('no_javascript')) {
      $form['#attached']['library'][] = array('views_ui', 'views_ui.admin');
    }

    $form['#attached']['css'] = static::getAdminCSS();

    $form['#attached']['js'][] = array(
      'data' => array('views' => array('ajax' => array(
        'id' => '#views-ajax-body',
        'title' => '#views-ajax-title',
        'popup' => '#views-ajax-popup',
        'defaultForm' => $view->getDefaultAJAXMessage(),
      ))),
      'type' => 'setting',
    );

    $form += array(
      '#prefix' => '',
      '#suffix' => '',
    );
    $form['#prefix'] .= '<div class="views-edit-view views-admin clearfix">';
    $form['#suffix'] = '</div>' . $form['#suffix'];

    $form['#attributes']['class'] = array('form-edit');

    if (isset($view->locked) && is_object($view->locked) && $view->locked->owner != $GLOBALS['user']->uid) {
      $form['locked'] = array(
        '#type' => 'container',
        '#attributes' => array('class' => array('view-locked', 'messages', 'warning')),
        '#children' => t('This view is being edited by user !user, and is therefore locked from editing by others. This lock is !age old. Click here to <a href="!break">break this lock</a>.', array('!user' => theme('username', array('account' => user_load($view->locked->owner))), '!age' => format_interval(REQUEST_TIME - $view->locked->updated), '!break' => url('admin/structure/views/view/' . $view->id() . '/break-lock'))),
        '#weight' => -10,
      );
    }
    else {
      if (isset($view->vid) && $view->vid == 'new') {
        $message = t('* All changes are stored temporarily. Click Save to make your changes permanent. Click Cancel to discard the view.');
      }
      else {
        $message = t('* All changes are stored temporarily. Click Save to make your changes permanent. Click Cancel to discard your changes.');
      }

      $form['changed'] = array(
        '#type' => 'container',
        '#attributes' => array('class' => array('view-changed', 'messages', 'warning')),
        '#children' => $message,
        '#weight' => -10,
      );
      if (empty($view->changed)) {
        $form['changed']['#attributes']['class'][] = 'js-hide';
      }
    }

    $form['help_text'] = array(
      '#type' => 'container',
      '#children' => t('Modify the display(s) of your view below or add new displays.'),
    );
    $form['displays'] = array(
      '#prefix' => '<h1 class="unit-title clearfix">' . t('Displays') . '</h1>',
      '#type' => 'container',
      '#attributes' => array(
        'class' => array(
          'views-displays',
        ),
      ),
    );


    $form['displays']['top'] = $this->renderDisplayTop($view);

    // The rest requires a display to be selected.
    if ($display_id) {
      $form_state['display_id'] = $display_id;

      // The part of the page where editing will take place.
      $form['displays']['settings'] = array(
        '#type' => 'container',
        '#id' => 'edit-display-settings',
      );
      $display_title = $this->getDisplayLabel($view, $display_id, FALSE);

      // Add a text that the display is disabled.
      if ($view->get('executable')->displayHandlers->has($display_id)) {
        if (!$view->get('executable')->displayHandlers->get($display_id)->isEnabled()) {
          $form['displays']['settings']['disabled']['#markup'] = t('This display is disabled.');
        }
      }

      // Add the edit display content
      $tab_content = $this->getDisplayTab($view);
      $tab_content['#theme_wrappers'] = array('container');
      $tab_content['#attributes'] = array('class' => array('views-display-tab'));
      $tab_content['#id'] = 'views-tab-' . $display_id;
      // Mark deleted displays as such.
      $display = $view->get('display');
      if (!empty($display[$display_id]['deleted'])) {
        $tab_content['#attributes']['class'][] = 'views-display-deleted';
      }
      // Mark disabled displays as such.

      if ($view->get('executable')->displayHandlers->has($display_id) && !$view->get('executable')->displayHandlers->get($display_id)->isEnabled()) {
        $tab_content['#attributes']['class'][] = 'views-display-disabled';
      }
      $form['displays']['settings']['settings_content'] = array(
        '#type' => 'container',
        'tab_content' => $tab_content,
      );

      // The content of the popup dialog.
      $form['ajax-area'] = array(
        '#type' => 'container',
        '#id' => 'views-ajax-popup',
      );
      $form['ajax-area']['ajax-title'] = array(
        '#markup' => '<h2 id="views-ajax-title"></h2>',
      );
      $form['ajax-area']['ajax-body'] = array(
        '#type' => 'container',
        '#id' => 'views-ajax-body',
        '#children' => $view->getDefaultAJAXMessage(),
      );
    }

    return $form;
  }

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::actions().
   */
  protected function actions(array $form, array &$form_state) {
    $actions = parent::actions($form, $form_state);
    unset($actions['delete']);

    $actions['cancel'] = array(
      '#value' => t('Cancel'),
      '#submit' => array(
        array($this, 'cancel'),
      ),
    );
    return $actions;
  }

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::validate().
   */
  public function validate(array $form, array &$form_state) {
    parent::validate($form, $form_state);

    $view = $this->getEntity($form_state);
    $errors = $view->get('executable')->validate();
    if ($errors !== TRUE) {
      foreach ($errors as $error) {
        form_set_error('', $error);
      }
    }
  }

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::submit().
   */
  public function submit(array $form, array &$form_state) {
    parent::submit($form, $form_state);

    $view = $this->getEntity($form_state);
    // Go through and remove displayed scheduled for removal.
    $displays = $view->get('display');
    foreach ($displays as $id => $display) {
      if (!empty($display['deleted'])) {
        $view->get('executable')->displayHandlers->remove($id);
        unset($displays[$id]);
      }
    }
    // Rename display ids if needed.
    foreach ($view->get('executable')->displayHandlers as $id => $display) {
      if (!empty($display->display['new_id'])) {
        $new_id = $display->display['new_id'];
        $view->get('executable')->displayHandlers->set($new_id, $view->get('executable')->displayHandlers->get($id));
        $view->get('executable')->displayHandlers->get($new_id)->display['id'] = $new_id;

        $displays[$new_id] = $displays[$id];
        unset($displays[$id]);
        // Redirect the user to the renamed display to be sure that the page itself exists and doesn't throw errors.
        $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $new_id;
      }
    }
    $view->set('display', $displays);

    // Direct the user to the right url, if the path of the display has changed.
    $query = drupal_container()->get('request')->query;
    // @todo: Revisit this when http://drupal.org/node/1668866 is in.
    $destination = $query->get('destination');
    if (!empty($destination)) {
      // Find out the first display which has a changed path and redirect to this url.
      $old_view = views_get_view($view->id());
      $old_view->initDisplay();
      foreach ($old_view->displayHandlers as $id => $display) {
        // Only check for displays with a path.
        $old_path = $display->getOption('path');
        if (empty($old_path)) {
          continue;
        }

        if (($display->getPluginId() == 'page') && ($old_path == $destination) && ($old_path != $view->get('executable')->displayHandlers->get($id)->getOption('path'))) {
          $destination = $view->get('executable')->displayHandlers->get($id)->getOption('path');
          $query->remove('destination');
          // @todo For whatever reason drupal_goto is still using $_GET.
          // @see http://drupal.org/node/1668866
          unset($_GET['destination']);
        }
      }
      $form_state['redirect'] = $destination;
    }

    $view->save();
    drupal_set_message(t('The view %name has been saved.', array('%name' => $view->getHumanName())));

    // Remove this view from cache so we can edit it properly.
    drupal_container()->get('user.tempstore')->get('views')->delete($view->id());
  }

  /**
   * Form submission handler for the 'cancel' action.
   *
   * @param array $form
   *   An associative array containing the structure of the form.
   * @param array $form_state
   *   A reference to a keyed array containing the current state of the form.
   */
  public function cancel(array $form, array &$form_state) {
    // Remove this view from cache so edits will be lost.
    $view = $this->getEntity($form_state);
    drupal_container()->get('user.tempstore')->get('views')->delete($view->id());
    $form_state['redirect'] = 'admin/structure/views';
  }

  /**
   * Overrides Drupal\Core\Entity\EntityFormController::actionsElement().
   */
  protected function actionsElement(array $form, array &$form_state) {
    $element = parent::actionsElement($form, $form_state);
    $element['#weight'] = 0;
    $view = $this->getEntity($form_state);
    if (empty($view->changed)) {
      $element['#attributes'] = array(
        'class' => array(
          'js-hide',
        ),
      );
    }
    return $element;
  }

  /**
   * Returns a renderable array representing the edit page for one display.
   */
  public function getDisplayTab($view) {
    $build = array();
    $display_id = $this->displayID;
    $display = $view->get('executable')->displayHandlers->get($display_id);
    // If the plugin doesn't exist, display an error message instead of an edit
    // page.
    if (empty($display)) {
      $title = isset($display->display['display_title']) ? $display->display['display_title'] : t('Invalid');
      // @TODO: Improved UX for the case where a plugin is missing.
      $build['#markup'] = t("Error: Display @display refers to a plugin named '@plugin', but that plugin is not available.", array('@display' => $display->display['id'], '@plugin' => $display->display['display_plugin']));
    }
    // Build the content of the edit page.
    else {
      $build['details'] = $this->getDisplayDetails($view, $display->display);
    }
    // In AJAX context, ViewUI::rebuildCurrentTab() returns this outside of form
    // context, so hook_form_views_ui_edit_form_alter() is insufficient.
    drupal_alter('views_ui_display_tab', $build, $view, $display_id);
    return $build;
  }

  /**
   * Helper function to get the display details section of the edit UI.
   *
   * @param $display
   *
   * @return array
   *   A renderable page build array.
   */
  public function getDisplayDetails($view, $display) {
    $display_title = $this->getDisplayLabel($view, $display['id'], FALSE);
    $build = array(
      '#theme_wrappers' => array('container'),
      '#attributes' => array('id' => 'edit-display-settings-details'),
    );

    $is_display_deleted = !empty($display['deleted']);
    // The master display cannot be cloned.
    $is_default = $display['id'] == 'default';
    // @todo: Figure out why getOption doesn't work here.
    $is_enabled = $view->get('executable')->displayHandlers->get($display['id'])->isEnabled();

    if ($display['id'] != 'default') {
      $build['top']['#theme_wrappers'] = array('container');
      $build['top']['#attributes']['id'] = 'edit-display-settings-top';
      $build['top']['#attributes']['class'] = array('views-ui-display-tab-actions', 'views-ui-display-tab-bucket', 'clearfix');

      // The Delete, Duplicate and Undo Delete buttons.
      $build['top']['actions'] = array(
        '#theme_wrappers' => array('dropbutton_wrapper'),
      );

      // Because some of the 'links' are actually submit buttons, we have to
      // manually wrap each item in <li> and the whole list in <ul>.
      $build['top']['actions']['prefix']['#markup'] = '<ul class="dropbutton">';

      if (!$is_display_deleted) {
        if (!$is_enabled) {
          $build['top']['actions']['enable'] = array(
            '#type' => 'submit',
            '#value' => t('enable @display_title', array('@display_title' => $display_title)),
            '#limit_validation_errors' => array(),
            '#submit' => array(array($this, 'submitDisplayEnable'), array($this, 'submitDelayDestination')),
            '#prefix' => '<li class="enable">',
            "#suffix" => '</li>',
          );
        }
        // Add a link to view the page.
        elseif ($view->get('executable')->displayHandlers->get($display['id'])->hasPath()) {
          $path = $view->get('executable')->displayHandlers->get($display['id'])->getPath();
          if (strpos($path, '%') === FALSE) {
            $build['top']['actions']['path'] = array(
              '#type' => 'link',
              '#title' => t('view @display', array('@display' => $display['display_title'])),
              '#options' => array('alt' => array(t("Go to the real page for this display"))),
              '#href' => $path,
              '#prefix' => '<li class="view">',
              "#suffix" => '</li>',
            );
          }
        }
        if (!$is_default) {
          $build['top']['actions']['duplicate'] = array(
            '#type' => 'submit',
            '#value' => t('clone @display_title', array('@display_title' => $display_title)),
            '#limit_validation_errors' => array(),
            '#submit' => array(array($this, 'submitDisplayDuplicate'), array($this, 'submitDelayDestination')),
            '#prefix' => '<li class="duplicate">',
            "#suffix" => '</li>',
          );
        }
        // Always allow a display to be deleted.
        $build['top']['actions']['delete'] = array(
          '#type' => 'submit',
          '#value' => t('delete @display_title', array('@display_title' => $display_title)),
          '#limit_validation_errors' => array(),
          '#submit' => array(array($this, 'submitDisplayDelete'), array($this, 'submitDelayDestination')),
          '#prefix' => '<li class="delete">',
          "#suffix" => '</li>',
        );
        if ($is_enabled) {
          $build['top']['actions']['disable'] = array(
            '#type' => 'submit',
            '#value' => t('disable @display_title', array('@display_title' => $display_title)),
            '#limit_validation_errors' => array(),
            '#submit' => array(array($this, 'submitDisplayDisable'), array($this, 'submitDelayDestination')),
            '#prefix' => '<li class="disable">',
            "#suffix" => '</li>',
          );
        }

        foreach (views_fetch_plugin_names('display', NULL, array($view->get('storage')->get('base_table'))) as $type => $label) {
          if ($type == $display['display_plugin']) {
            continue;
          }

          $build['top']['actions']['clone_as'][$type] = array(
            '#type' => 'submit',
            '#value' => t('clone as @type', array('@type' => $label)),
            '#limit_validation_errors' => array(),
            '#submit' => array(array($this, 'submitCloneDisplayAsType'), array($this, 'submitDelayDestination')),
            '#prefix' => '<li class="duplicate">',
            '#suffix' => '</li>',
          );
        }
      }
      else {
        $build['top']['actions']['undo_delete'] = array(
          '#type' => 'submit',
          '#value' => t('undo delete of @display_title', array('@display_title' => $display_title)),
          '#limit_validation_errors' => array(),
          '#submit' => array(array($this, 'submitDisplayUndoDelete'), array($this, 'submitDelayDestination')),
          '#prefix' => '<li class="undo-delete">',
          "#suffix" => '</li>',
        );
      }
      $build['top']['actions']['suffix']['#markup'] = '</ul>';

      // The area above the three columns.
      $build['top']['display_title'] = array(
        '#theme' => 'views_ui_display_tab_setting',
        '#description' => t('Display name'),
        '#link' => $view->get('executable')->displayHandlers->get($display['id'])->optionLink(check_plain($display_title), 'display_title'),
      );
    }

    $build['columns'] = array();
    $build['columns']['#theme_wrappers'] = array('container');
    $build['columns']['#attributes'] = array('id' => 'edit-display-settings-main', 'class' => array('clearfix', 'views-display-columns'));

    $build['columns']['first']['#theme_wrappers'] = array('container');
    $build['columns']['first']['#attributes'] = array('class' => array('views-display-column', 'first'));

    $build['columns']['second']['#theme_wrappers'] = array('container');
    $build['columns']['second']['#attributes'] = array('class' => array('views-display-column', 'second'));

    $build['columns']['second']['settings'] = array();
    $build['columns']['second']['header'] = array();
    $build['columns']['second']['footer'] = array();
    $build['columns']['second']['pager'] = array();

    // The third column buckets are wrapped in details.
    $build['columns']['third'] = array(
      '#type' => 'details',
      '#title' => t('Advanced'),
      '#collapsed' => TRUE,
      '#theme_wrappers' => array('details', 'container'),
      '#attributes' => array(
        'class' => array(
          'views-display-column',
          'third',
        ),
      ),
    );

    // Collapse the details by default.
    if (config('views.settings')->get('ui.show.advanced_column')) {
      $build['columns']['third']['#collapsed'] = FALSE;
    }

    // Each option (e.g. title, access, display as grid/table/list) fits into one
    // of several "buckets," or boxes (Format, Fields, Sort, and so on).
    $buckets = array();

    // Fetch options from the display plugin, with a list of buckets they go into.
    $options = array();
    $view->get('executable')->displayHandlers->get($display['id'])->optionsSummary($buckets, $options);

    // Place each option into its bucket.
    foreach ($options as $id => $option) {
      // Each option self-identifies as belonging in a particular bucket.
      $buckets[$option['category']]['build'][$id] = $this->buildOptionForm($view, $id, $option, $display);
    }

    // Place each bucket into the proper column.
    foreach ($buckets as $id => $bucket) {
      // Let buckets identify themselves as belonging in a column.
      if (isset($bucket['column']) && isset($build['columns'][$bucket['column']])) {
        $column = $bucket['column'];
      }
      // If a bucket doesn't pick one of our predefined columns to belong to, put
      // it in the last one.
      else {
        $column = 'third';
      }
      if (isset($bucket['build']) && is_array($bucket['build'])) {
        $build['columns'][$column][$id] = $bucket['build'];
        $build['columns'][$column][$id]['#theme_wrappers'][] = 'views_ui_display_tab_bucket';
        $build['columns'][$column][$id]['#title'] = !empty($bucket['title']) ? $bucket['title'] : '';
        $build['columns'][$column][$id]['#name'] = !empty($bucket['title']) ? $bucket['title'] : $id;
      }
    }

    $build['columns']['first']['fields'] = $this->getFormBucket($view, 'field', $display);
    $build['columns']['first']['filters'] = $this->getFormBucket($view, 'filter', $display);
    $build['columns']['first']['sorts'] = $this->getFormBucket($view, 'sort', $display);
    $build['columns']['second']['header'] = $this->getFormBucket($view, 'header', $display);
    $build['columns']['second']['footer'] = $this->getFormBucket($view, 'footer', $display);
    $build['columns']['third']['arguments'] = $this->getFormBucket($view, 'argument', $display);
    $build['columns']['third']['relationships'] = $this->getFormBucket($view, 'relationship', $display);
    $build['columns']['third']['empty'] = $this->getFormBucket($view, 'empty', $display);

    return $build;
  }

  /**
   * Submit handler to add a restore a removed display to a view.
   */
  public function submitDisplayUndoDelete($form, &$form_state) {
    $view = $this->getEntity($form_state);
    // Create the new display
    $id = $form_state['display_id'];
    $displays = $view->get('display');
    $displays[$id]['deleted'] = FALSE;
    $view->set('display', $displays);

    // Store in cache
    views_ui_cache_set($view);

    // Redirect to the top-level edit page.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $id;
  }

  /**
   * Submit handler to enable a disabled display.
   */
  public function submitDisplayEnable($form, &$form_state) {
    $view = $this->getEntity($form_state);
    $id = $form_state['display_id'];
    // setOption doesn't work because this would might affect upper displays
    $view->get('executable')->displayHandlers->get($id)->setOption('enabled', TRUE);

    // Store in cache
    views_ui_cache_set($view);

    // Redirect to the top-level edit page.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $id;
  }

  /**
   * Submit handler to disable display.
   */
  public function submitDisplayDisable($form, &$form_state) {
    $view = $this->getEntity($form_state);
    $id = $form_state['display_id'];
    $view->get('executable')->displayHandlers->get($id)->setOption('enabled', FALSE);

    // Store in cache
    views_ui_cache_set($view);

    // Redirect to the top-level edit page.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $id;
  }

  /**
   * Submit handler to delete a display from a view.
   */
  public function submitDisplayDelete($form, &$form_state) {
    $view = $this->getEntity($form_state);
    $display_id = $form_state['display_id'];

    // Mark the display for deletion.
    $displays = $view->get('display');
    $displays[$display_id]['deleted'] = TRUE;
    $view->set('display', $displays);
    views_ui_cache_set($view);

    // Redirect to the top-level edit page. The first remaining display will
    // become the active display.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id();
  }

  /**
   * Regenerate the current tab for AJAX updates.
   *
   * @param \Drupal\views_ui\ViewUI $view
   *   The view to regenerate its tab.
   * @param \Drupal\Core\Ajax\AjaxResponse $response
   *   The response object to add new commands to.
   * @param string $display_id
   *   The display ID of the tab to regenerate.
   */
  public function rebuildCurrentTab(ViewUI $view, AjaxResponse $response, $display_id) {
    $this->displayID = $display_id;
    if (!$view->get('executable')->setDisplay('default')) {
      return;
    }

    // Regenerate the main display area.
    $build = $this->getDisplayTab($view);
    static::addMicroweights($build);
    $response->addCommand(new HtmlCommand('#views-tab-' . $display_id, drupal_render($build)));

    // Regenerate the top area so changes to display names and order will appear.
    $build = $this->renderDisplayTop($view);
    static::addMicroweights($build);
    $response->addCommand(new ReplaceCommand('#views-display-top', drupal_render($build)));
  }

  /**
   * Render the top of the display so it can be updated during ajax operations.
   */
  public function renderDisplayTop(ViewUI $view) {
    $display_id = $this->displayID;
    $element['#theme_wrappers'][] = 'views_ui_container';
    $element['#attributes']['class'] = array('views-display-top', 'clearfix');
    $element['#attributes']['id'] = array('views-display-top');

    // Extra actions for the display
    $element['extra_actions'] = array(
      '#type' => 'dropbutton',
      '#attributes' => array(
        'id' => 'views-display-extra-actions',
      ),
      '#links' => array(
        'edit-details' => array(
          'title' => t('edit view name/description'),
          'href' => "admin/structure/views/nojs/edit-details/{$view->id()}/$display_id",
          'attributes' => array('class' => array('views-ajax-link')),
        ),
        'analyze' => array(
          'title' => t('analyze view'),
          'href' => "admin/structure/views/nojs/analyze/{$view->id()}/$display_id",
          'attributes' => array('class' => array('views-ajax-link')),
        ),
        'clone' => array(
          'title' => t('clone view'),
          'href' => "admin/structure/views/view/{$view->id()}/clone",
        ),
        'reorder' => array(
          'title' => t('reorder displays'),
          'href' => "admin/structure/views/nojs/reorder-displays/{$view->id()}/$display_id",
          'attributes' => array('class' => array('views-ajax-link')),
        ),
      ),
    );

    // Let other modules add additional links here.
    drupal_alter('views_ui_display_top_links', $element['extra_actions']['#links'], $view, $display_id);

    if (isset($view->type) && $view->type != t('Default')) {
      if ($view->type == t('Overridden')) {
        $element['extra_actions']['#links']['revert'] = array(
          'title' => t('revert view'),
          'href' => "admin/structure/views/view/{$view->id()}/revert",
          'query' => array('destination' => "admin/structure/views/view/{$view->id()}"),
        );
      }
      else {
        $element['extra_actions']['#links']['delete'] = array(
          'title' => t('delete view'),
          'href' => "admin/structure/views/view/{$view->id()}/delete",
        );
      }
    }

    // Determine the displays available for editing.
    if ($tabs = $this->getDisplayTabs($view)) {
      if ($display_id) {
        $tabs[$display_id]['#active'] = TRUE;
      }
      $tabs['#prefix'] = '<h2 class="element-invisible">' . t('Secondary tabs') . '</h2><ul id = "views-display-menu-tabs" class="tabs secondary">';
      $tabs['#suffix'] = '</ul>';
      $element['tabs'] = $tabs;
    }

    // Buttons for adding a new display.
    foreach (views_fetch_plugin_names('display', NULL, array($view->get('base_table'))) as $type => $label) {
      $element['add_display'][$type] = array(
        '#type' => 'submit',
        '#value' => t('Add !display', array('!display' => $label)),
        '#limit_validation_errors' => array(),
        '#submit' => array(array($this, 'submitDisplayAdd'), array($this, 'submitDelayDestination')),
        '#attributes' => array('class' => array('add-display')),
        // Allow JavaScript to remove the 'Add ' prefix from the button label when
        // placing the button in a "Add" dropdown menu.
        '#process' => array_merge(array('views_ui_form_button_was_clicked'), element_info_property('submit', '#process', array())),
        '#values' => array(t('Add !display', array('!display' => $label)), $label),
      );
    }

    return $element;
  }

  /**
   * Submit handler for form buttons that do not complete a form workflow.
   *
   * The Edit View form is a multistep form workflow, but with state managed by
   * the TempStore rather than $form_state['rebuild']. Without this
   * submit handler, buttons that add or remove displays would redirect to the
   * destination parameter (e.g., when the Edit View form is linked to from a
   * contextual link). This handler can be added to buttons whose form submission
   * should not yet redirect to the destination.
   */
  public function submitDelayDestination($form, &$form_state) {
    $query = drupal_container()->get('request')->query;
    // @todo: Revisit this when http://drupal.org/node/1668866 is in.
    $destination = $query->get('destination');
    if (isset($destination) && $form_state['redirect'] !== FALSE) {
      if (!isset($form_state['redirect'])) {
        $form_state['redirect'] = current_path();
      }
      if (is_string($form_state['redirect'])) {
        $form_state['redirect'] = array($form_state['redirect']);
      }
      $options = isset($form_state['redirect'][1]) ? $form_state['redirect'][1] : array();
      if (!isset($options['query']['destination'])) {
        $options['query']['destination'] = $destination;
      }
      $form_state['redirect'][1] = $options;
      $query->remove('destination');
    }
  }

  /**
   * Submit handler to duplicate a display for a view.
   */
  public function submitDisplayDuplicate($form, &$form_state) {
    $view = $this->getEntity($form_state);
    $display_id = $this->displayID;

    // Create the new display.
    $displays = $view->get('display');
    $new_display_id = $view->addDisplay($displays[$display_id]['display_plugin']);
    $displays[$new_display_id] = $displays[$display_id];
    $displays[$new_display_id]['id'] = $new_display_id;
    $view->set('display', $displays);

    // By setting the current display the changed marker will appear on the new
    // display.
    $view->get('executable')->current_display = $new_display_id;
    views_ui_cache_set($view);

    // Redirect to the new display's edit page.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $new_display_id;
  }

  /**
   * Submit handler to add a display to a view.
   */
  public function submitDisplayAdd($form, &$form_state) {
    $view = $this->getEntity($form_state);
    // Create the new display.
    $parents = $form_state['triggering_element']['#parents'];
    $display_type = array_pop($parents);
    $display_id = $view->addDisplay($display_type);
    // A new display got added so the asterisks symbol should appear on the new
    // display.
    $view->get('executable')->current_display = $display_id;
    views_ui_cache_set($view);

    // Redirect to the new display's edit page.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $display_id;
  }

  /**
   * Submit handler to clone a display as another display type.
   */
  public function submitCloneDisplayAsType($form, &$form_state) {
    $view = $this->getEntity($form_state);
    $display_id = $this->displayID;

    // Create the new display.
    $parents = $form_state['triggering_element']['#parents'];
    $display_type = array_pop($parents);
    $new_display_id = $view->addDisplay($display_type);
    $displays = $view->get('display');

    // Let the display title be generated by the addDisplay method and set the
    // right display plugin, but keep the rest from the original display.
    $display_clone = $displays[$display_id];
    unset($display_clone['display_title']);
    unset($display_clone['display_plugin']);

    $displays[$new_display_id] = NestedArray::mergeDeep($displays[$new_display_id], $display_clone);
    $displays[$new_display_id]['id'] = $new_display_id;
    $view->set('display', $displays);

    // By setting the current display the changed marker will appear on the new
    // display.
    $view->get('executable')->current_display = $new_display_id;
    views_ui_cache_set($view);

    // Redirect to the new display's edit page.
    $form_state['redirect'] = 'admin/structure/views/view/' . $view->id() . '/edit/' . $new_display_id;
  }

  /**
   * Build a renderable array representing one option on the edit form.
   *
   * This function might be more logical as a method on an object, if a suitable
   * object emerges out of refactoring.
   */
  public function buildOptionForm(ViewUI $view, $id, $option, $display) {
    $option_build = array();
    $option_build['#theme'] = 'views_ui_display_tab_setting';

    $option_build['#description'] = $option['title'];

    $option_build['#link'] = $view->get('executable')->displayHandlers->get($display['id'])->optionLink($option['value'], $id, '', empty($option['desc']) ? '' : $option['desc']);

    $option_build['#links'] = array();
    if (!empty($option['links']) && is_array($option['links'])) {
      foreach ($option['links'] as $link_id => $link_value) {
        $option_build['#settings_links'][] = $view->get('executable')->displayHandlers->get($display['id'])->optionLink($option['setting'], $link_id, 'views-button-configure', $link_value);
      }
    }

    if (!empty($view->get('executable')->displayHandlers->get($display['id'])->options['defaults'][$id])) {
      $display_id = 'default';
      $option_build['#defaulted'] = TRUE;
    }
    else {
      $display_id = $display['id'];
      if (!$view->get('executable')->displayHandlers->get($display['id'])->isDefaultDisplay()) {
        if ($view->get('executable')->displayHandlers->get($display['id'])->defaultableSections($id)) {
          $option_build['#overridden'] = TRUE;
        }
      }
    }
    $option_build['#attributes']['class'][] = drupal_clean_css_identifier($display_id . '-' . $id);
    return $option_build;
  }

  /**
   * Add information about a section to a display.
   */
  public function getFormBucket(ViewUI $view, $type, $display) {
    $build = array(
      '#theme_wrappers' => array('views_ui_display_tab_bucket'),
    );
    $types = ViewExecutable::viewsHandlerTypes();

    $build['#overridden'] = FALSE;
    $build['#defaulted'] = FALSE;

    $build['#name'] = $build['#title'] = $types[$type]['title'];

    $rearrange_url = "admin/structure/views/nojs/rearrange/{$view->id()}/{$display['id']}/$type";
    $rearrange_text = t('Rearrange');
    $class = 'icon compact rearrange';

    // Different types now have different rearrange forms, so we use this switch
    // to get the right one.
    switch ($type) {
      case 'filter':
        // The rearrange form for filters contains the and/or UI, so override
        // the used path.
        $rearrange_url = "admin/structure/views/nojs/rearrange-filter/{$view->id()}/{$display['id']}";
        $rearrange_text = t('And/Or, Rearrange');
        // TODO: Add another class to have another symbol for filter rearrange.
        $class = 'icon compact rearrange';
        break;
      case 'field':
        // Fetch the style plugin info so we know whether to list fields or not.
        $style_plugin = $view->get('executable')->displayHandlers->get($display['id'])->getPlugin('style');
        $uses_fields = $style_plugin && $style_plugin->usesFields();
        if (!$uses_fields) {
          $build['fields'][] = array(
            '#markup' => t('The selected style or row format does not utilize fields.'),
            '#theme_wrappers' => array('views_ui_container'),
            '#attributes' => array('class' => array('views-display-setting')),
          );
          return $build;
        }
        break;
      case 'header':
      case 'footer':
      case 'empty':
        if (!$view->get('executable')->displayHandlers->get($display['id'])->usesAreas()) {
          $build[$type][] = array(
            '#markup' => t('The selected display type does not utilize @type plugins', array('@type' => $type)),
            '#theme_wrappers' => array('views_ui_container'),
            '#attributes' => array('class' => array('views-display-setting')),
          );
          return $build;
        }
        break;
    }

    // Create an array of actions to pass to theme_links
    $actions = array();
    $count_handlers = count($view->get('executable')->displayHandlers->get($display['id'])->getHandlers($type));
    $actions['add'] = array(
      'title' => t('Add'),
      'href' => "admin/structure/views/nojs/add-item/{$view->id()}/{$display['id']}/$type",
      'attributes' => array('class' => array('icon compact add', 'views-ajax-link'), 'title' => t('Add'), 'id' => 'views-add-' . $type),
      'html' => TRUE,
    );
    if ($count_handlers > 0) {
      $actions['rearrange'] = array(
        'title' => $rearrange_text,
        'href' => $rearrange_url,
        'attributes' => array('class' => array($class, 'views-ajax-link'), 'title' => t('Rearrange'), 'id' => 'views-rearrange-' . $type),
        'html' => TRUE,
      );
    }

    // Render the array of links
    $build['#actions'] = array(
      '#type' => 'dropbutton',
      '#links' => $actions,
      '#attributes' => array(
        'class' => array('views-ui-settings-bucket-operations'),
      ),
    );

    if (!$view->get('executable')->displayHandlers->get($display['id'])->isDefaultDisplay()) {
      if (!$view->get('executable')->displayHandlers->get($display['id'])->isDefaulted($types[$type]['plural'])) {
        $build['#overridden'] = TRUE;
      }
      else {
        $build['#defaulted'] = TRUE;
      }
    }

    static $relationships = NULL;
    if (!isset($relationships)) {
      // Get relationship labels
      $relationships = array();
      foreach ($view->get('executable')->displayHandlers->get($display['id'])->getHandlers('relationship') as $id => $handler) {
        $relationships[$id] = $handler->label();
      }
    }

    // Filters can now be grouped so we do a little bit extra:
    $groups = array();
    $grouping = FALSE;
    if ($type == 'filter') {
      $group_info = $view->get('executable')->displayHandlers->get('default')->getOption('filter_groups');
      // If there is only one group but it is using the "OR" filter, we still
      // treat it as a group for display purposes, since we want to display the
      // "OR" label next to items within the group.
      if (!empty($group_info['groups']) && (count($group_info['groups']) > 1 || current($group_info['groups']) == 'OR')) {
        $grouping = TRUE;
        $groups = array(0 => array());
      }
    }

    $build['fields'] = array();

    foreach ($view->get('executable')->displayHandlers->get($display['id'])->getOption($types[$type]['plural']) as $id => $field) {
      // Build the option link for this handler ("Node: ID = article").
      $build['fields'][$id] = array();
      $build['fields'][$id]['#theme'] = 'views_ui_display_tab_setting';

      $handler = $view->get('executable')->displayHandlers->get($display['id'])->getHandler($type, $id);
      if (empty($handler)) {
        $build['fields'][$id]['#class'][] = 'broken';
        $field_name = t('Broken/missing handler: @table > @field', array('@table' => $field['table'], '@field' => $field['field']));
        $build['fields'][$id]['#link'] = l($field_name, "admin/structure/views/nojs/config-item/{$view->id()}/{$display['id']}/$type/$id", array('attributes' => array('class' => array('views-ajax-link')), 'html' => TRUE));
        continue;
      }

      $field_name = check_plain($handler->adminLabel(TRUE));
      if (!empty($field['relationship']) && !empty($relationships[$field['relationship']])) {
        $field_name = '(' . $relationships[$field['relationship']] . ') ' . $field_name;
      }

      $description = filter_xss_admin($handler->adminSummary());
      $link_text = $field_name . (empty($description) ? '' : " ($description)");
      $link_attributes = array('class' => array('views-ajax-link'));
      if (!empty($field['exclude'])) {
        $link_attributes['class'][] = 'views-field-excluded';
      }
      $build['fields'][$id]['#link'] = l($link_text, "admin/structure/views/nojs/config-item/{$view->id()}/{$display['id']}/$type/$id", array('attributes' => $link_attributes, 'html' => TRUE));
      $build['fields'][$id]['#class'][] = drupal_clean_css_identifier($display['id']. '-' . $type . '-' . $id);

      if ($view->get('executable')->displayHandlers->get($display['id'])->useGroupBy() && $handler->usesGroupBy()) {
        $build['fields'][$id]['#settings_links'][] = l('<span class="label">' . t('Aggregation settings') . '</span>', "admin/structure/views/nojs/config-item-group/{$view->id()}/{$display['id']}/$type/$id", array('attributes' => array('class' => 'views-button-configure views-ajax-link', 'title' => t('Aggregation settings')), 'html' => TRUE));
      }

      if ($handler->hasExtraOptions()) {
        $build['fields'][$id]['#settings_links'][] = l('<span class="label">' . t('Settings') . '</span>', "admin/structure/views/nojs/config-item-extra/{$view->id()}/{$display['id']}/$type/$id", array('attributes' => array('class' => array('views-button-configure', 'views-ajax-link'), 'title' => t('Settings')), 'html' => TRUE));
      }

      if ($grouping) {
        $gid = $handler->options['group'];

        // Show in default group if the group does not exist.
        if (empty($group_info['groups'][$gid])) {
          $gid = 0;
        }
        $groups[$gid][] = $id;
      }
    }

    // If using grouping, re-order fields so that they show up properly in the list.
    if ($type == 'filter' && $grouping) {
      $store = $build['fields'];
      $build['fields'] = array();
      foreach ($groups as $gid => $contents) {
        // Display an operator between each group.
        if (!empty($build['fields'])) {
          $build['fields'][] = array(
            '#theme' => 'views_ui_display_tab_setting',
            '#class' => array('views-group-text'),
            '#link' => ($group_info['operator'] == 'OR' ? t('OR') : t('AND')),
          );
        }
        // Display an operator between each pair of filters within the group.
        $keys = array_keys($contents);
        $last = end($keys);
        foreach ($contents as $key => $pid) {
          if ($key != $last) {
            $store[$pid]['#link'] .= '&nbsp;&nbsp;' . ($group_info['groups'][$gid] == 'OR' ? t('OR') : t('AND'));
          }
          $build['fields'][$pid] = $store[$pid];
        }
      }
    }

    return $build;
  }

  /**
   * Recursively adds microweights to a render array, similar to what form_builder() does for forms.
   *
   * @todo Submit a core patch to fix drupal_render() to do this, so that all
   *   render arrays automatically preserve array insertion order, as forms do.
   */
  public static function addMicroweights(&$build) {
    $count = 0;
    foreach (element_children($build) as $key) {
      if (!isset($build[$key]['#weight'])) {
        $build[$key]['#weight'] = $count/1000;
      }
      static::addMicroweights($build[$key]);
      $count++;
    }
  }

}
