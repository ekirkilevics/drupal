<?php

/**
 * @file
 * Theming for maintenance pages.
 */

/**
 * Sets up the theming system for maintenance page.
 *
 * Used for site installs, updates and when the site is in maintenance mode.
 * It also applies when the database is unavailable or bootstrap was not
 * complete. Seven is always used for the initial install and update
 * operations. In other cases, Bartik is used, but this can be overridden by
 * setting a "maintenance_theme" key in the $conf variable in settings.php.
 */
function _drupal_maintenance_theme() {
  global $theme, $theme_key, $conf;

  // If $theme is already set, assume the others are set too, and do nothing.
  if (isset($theme)) {
    return;
  }

  require_once DRUPAL_ROOT . '/' . settings()->get('path_inc', 'core/includes/path.inc');
  require_once DRUPAL_ROOT . '/core/includes/theme.inc';
  require_once DRUPAL_ROOT . '/core/includes/common.inc';
  require_once DRUPAL_ROOT . '/core/includes/unicode.inc';
  require_once DRUPAL_ROOT . '/core/includes/file.inc';
  require_once DRUPAL_ROOT . '/core/includes/module.inc';
  unicode_check();

  // Install and update pages are treated differently to prevent theming overrides.
  if (defined('MAINTENANCE_MODE') && (MAINTENANCE_MODE == 'install' || MAINTENANCE_MODE == 'update')) {
    $custom_theme = (isset($conf['maintenance_theme']) ? $conf['maintenance_theme'] : 'seven');
  }
  else {
    // The bootstrap was not complete. So we are operating in a crippled
    // environment, we need to bootstrap just enough to allow hook invocations
    // to work. See _drupal_log_error().
    if (!class_exists('Drupal\Core\Database\Database', FALSE)) {
      require_once DRUPAL_ROOT . '/core/includes/database.inc';
    }

    // We use the default theme as the maintenance theme. If a default theme
    // isn't specified in the database or in settings.php, we use Bartik.
    // @todo Should use the actual default theme configured, but that depends on
    //   configuration being available while possibly not having a working
    //   database connection (yet). And only if that fails, should fall back to
    //   Stark otherwise. Since there is no low-level access to configuration
    //   currently, we only consult settings.php and fall back to Bartik
    //   otherwise, as it looks generic enough and way more user-friendly.
    $custom_theme = variable_get('maintenance_theme', config('system.theme')->get('default')) ?: 'bartik';
  }

  // Ensure that system.module is loaded.
  if (!function_exists('_system_rebuild_theme_data')) {
    $module_list['system'] = 'core/modules/system/system.module';
    $module_handler = drupal_container()->get('module_handler');
    $module_handler->setModuleList($module_list);
    $module_handler->load('system');
  }

  $themes = list_themes();

  // list_themes() triggers a drupal_alter() in maintenance mode, but we can't
  // let themes alter the .info data until we know a theme's base themes. So
  // don't set global $theme until after list_themes() builds its cache.
  $theme = $custom_theme;

  // Store the identifier for retrieving theme settings with.
  $theme_key = $theme;

  // Find all our ancestor themes and put them in an array.
  $base_theme = array();
  $ancestor = $theme;
  while ($ancestor && isset($themes[$ancestor]->base_theme)) {
    $base_theme[] = $new_base_theme = $themes[$themes[$ancestor]->base_theme];
    $ancestor = $themes[$ancestor]->base_theme;
  }
  _drupal_theme_initialize($themes[$theme], array_reverse($base_theme), '_theme_load_offline_registry');

  // These are usually added from system_init() -except maintenance.css.
  // When the database is inactive it's not called so we add it here.
  $path = drupal_get_path('module', 'system');
  drupal_add_css($path . '/system.base.css');
  drupal_add_css($path . '/system.admin.css');
  drupal_add_css($path . '/system.theme.css');
  drupal_add_css($path . '/system.maintenance.css');
}

/**
 * Builds the registry when the site needs to bypass any database calls.
 */
function _theme_load_offline_registry($theme, $base_theme = NULL, $theme_engine = NULL) {
  return _theme_build_registry($theme, $base_theme, $theme_engine);
}

/**
 * Returns HTML for a list of maintenance tasks to perform.
 *
 * @param $variables
 *   An associative array containing:
 *   - items: An associative array of maintenance tasks.
 *   - active: The key for the currently active maintenance task.
 *
 * @ingroup themeable
 */
function theme_task_list($variables) {
  $t = get_t();
  $items = $variables['items'];
  $active = $variables['active'];

  $done = isset($items[$active]) || $active == NULL;
  $output = '<h2 class="element-invisible">Installation tasks</h2>';
  $output .= '<ol class="task-list">';

  foreach ($items as $k => $item) {
    if ($active == $k) {
      $class = 'active';
      $status = '(' . $t('active') . ')';
      $done = FALSE;
    }
    else {
      $class = $done ? 'done' : '';
      $status = $done ? '(' . $t('done') . ')' : '';
    }
    $output .= '<li';
    $output .= ($class ? ' class="' . $class . '"' : '') . '>';
    $output .= $item;
    $output .= ($status ? '<span class="element-invisible">' . $status . '</span>' : '');
    $output .= '</li>';
  }
  $output .= '</ol>';
  return $output;
}

/**
 * Returns HTML for the installation page.
 *
 * Note: this function is not themeable.
 *
 * @param $variables
 *   An associative array containing:
 *   - content: The page content to show.
 */
function theme_install_page($variables) {
  drupal_add_http_header('Content-Type', 'text/html; charset=utf-8');
  return theme('maintenance_page', $variables);
}

/**
 * Returns HTML for the update page.
 *
 * Note: this function is not themeable.
 *
 * @param $variables
 *   An associative array containing:
 *   - content: The page content to show.
 *   - show_messages: Whether to output status and error messages.
 *     FALSE can be useful to postpone the messages to a subsequent page.
 */
function theme_update_page($variables) {
  drupal_add_http_header('Content-Type', 'text/html; charset=utf-8');
  return theme('maintenance_page', $variables);
}

/**
 * Returns HTML for a results report of an operation run by authorize.php.
 *
 * @param $variables
 *   An associative array containing:
 *   - messages: An array of result messages.
 *
 * @ingroup themeable
 */
function theme_authorize_report($variables) {
  $messages = $variables['messages'];
  $output = '';
  if (!empty($messages)) {
    $output .= '<div id="authorize-results">';
    foreach ($messages as $heading => $logs) {
      $items = array();
      foreach ($logs as $number => $log_message) {
        if ($number === '#abort') {
          continue;
        }
        $items[] = theme('authorize_message', array('message' => $log_message['message'], 'success' => $log_message['success']));
      }
      $output .= theme('item_list',  array('items' => $items, 'title' => $heading));
    }
    $output .= '</div>';
  }
  return $output;
}

/**
 * Returns HTML for a single log message from the authorize.php batch operation.
 *
 * @param $variables
 *   An associative array containing:
 *   - message: The log message.
 *   - success: A boolean indicating failure or success.
 *
 * @ingroup themeable
 */
function theme_authorize_message($variables) {
  $message = $variables['message'];
  $success = $variables['success'];
  if ($success) {
    $item = array('data' => $message, 'class' => array('success'));
  }
  else {
    $item = array('data' => '<strong>' . $message . '</strong>', 'class' => array('failure'));
  }
  return $item;
}
