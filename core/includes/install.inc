<?php

 /**
 * @file
 * API functions for installing modules and themes.
 */

use Drupal\Core\Database\Database;
use Drupal\Core\DrupalKernel;
use Drupal\locale\Gettext;

/**
 * Requirement severity -- Informational message only.
 */
const REQUIREMENT_INFO = -1;

/**
 * Requirement severity -- Requirement successfully met.
 */
const REQUIREMENT_OK = 0;

/**
 * Requirement severity -- Warning condition; proceed but flag warning.
 */
const REQUIREMENT_WARNING = 1;

/**
 * Requirement severity -- Error condition; abort installation.
 */
const REQUIREMENT_ERROR = 2;

/**
 * File permission check -- File exists.
 */
const FILE_EXIST = 1;

/**
 * File permission check -- File is readable.
 */
const FILE_READABLE = 2;

/**
 * File permission check -- File is writable.
 */
const FILE_WRITABLE = 4;

/**
 * File permission check -- File is executable.
 */
const FILE_EXECUTABLE = 8;

/**
 * File permission check -- File does not exist.
 */
const FILE_NOT_EXIST = 16;

/**
 * File permission check -- File is not readable.
 */
const FILE_NOT_READABLE = 32;

/**
 * File permission check -- File is not writable.
 */
const FILE_NOT_WRITABLE = 64;

/**
 * File permission check -- File is not executable.
 */
const FILE_NOT_EXECUTABLE = 128;

/**
 * Loads .install files for installed modules to initialize the update system.
 */
function drupal_load_updates() {
  foreach (drupal_get_installed_schema_version(NULL, FALSE, TRUE) as $module => $schema_version) {
    if ($schema_version > -1) {
      module_load_install($module);
    }
  }
}

/**
 * Loads the installation profile, extracting its defined distribution name.
 *
 * @return
 *   The distribution name defined in the profile's .info file. Defaults to
 *   "Drupal" if none is explicitly provided by the installation profile.
 *
 * @see install_profile_info()
 */
function drupal_install_profile_distribution_name() {
  // During installation, the profile information is stored in the global
  // installation state (it might not be saved anywhere yet).
  if (drupal_installation_attempted()) {
    global $install_state;
    return $install_state['profile_info']['distribution_name'];
  }
  // At all other times, we load the profile via standard methods.
  else {
    $profile = drupal_get_profile();
    $info = system_get_info('module', $profile);
    return $info['distribution_name'];
  }
}

/**
 * Detects all supported databases that are compiled into PHP.
 *
 * @return
 *  An array of database types compiled into PHP.
 */
function drupal_detect_database_types() {
  $databases = drupal_get_database_types();

  foreach ($databases as $driver => $installer) {
    $databases[$driver] = $installer->name();
  }

  return $databases;
}

/**
 * Returns all supported database installer objects that are compiled into PHP.
 *
 * @return
 *  An array of database installer objects compiled into PHP.
 */
function drupal_get_database_types() {
  $databases = array();
  $drivers = array();

  // We define a driver as a directory in /core/includes/database that in turn
  // contains a database.inc file. That allows us to drop in additional drivers
  // without modifying the installer.
  require_once DRUPAL_ROOT . '/core/includes/database.inc';
  // Allow any valid PHP identifier.
  // @see http://www.php.net/manual/en/language.variables.basics.php.
  $mask = '/^[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*$/';
  $files = file_scan_directory(DRUPAL_ROOT . '/core/lib/Drupal/Core/Database/Driver', $mask, array('recurse' => FALSE));
  if (is_dir(DRUPAL_ROOT . '/drivers/lib/Drupal/Driver/Database')) {
    $files += file_scan_directory(DRUPAL_ROOT . '/drivers/lib/Drupal/Driver/Database/', $mask, array('recurse' => FALSE));
  }
  foreach ($files as $file) {
    if (file_exists($file->uri . '/Install/Tasks.php')) {
      $drivers[$file->filename] = $file->uri;
    }
  }
  foreach ($drivers as $driver => $file) {
    $installer = db_installer_object($driver);
    if ($installer->installable()) {
      $databases[$driver] = $installer;
    }
  }

  // Usability: unconditionally put the MySQL driver on top.
  if (isset($databases['mysql'])) {
    $mysql_database = $databases['mysql'];
    unset($databases['mysql']);
    $databases = array('mysql' => $mysql_database) + $databases;
  }

  return $databases;
}

/**
 * Replaces values in settings.php with values in the submitted array.
 *
 * @param $settings
 *   An array of settings that need to be updated.
 */
function drupal_rewrite_settings($settings = array()) {
  drupal_static_reset('conf_path');
  $settings_file = conf_path(FALSE) . '/settings.php';

  // Build list of setting names and insert the values into the global namespace.
  $keys = array();
  foreach ($settings as $setting => $data) {
    $GLOBALS[$setting] = $data['value'];
    $keys[] = $setting;
  }

  $buffer = NULL;
  $contents = file_get_contents(DRUPAL_ROOT . '/' . $settings_file);
  if ($contents !== FALSE) {
    // Step through each token in settings.php and replace any variables that
    // are in the passed-in array.
    $replacing_variable = FALSE;
    foreach (token_get_all($contents) as $token) {
      // Strip off the leading "$" before comparing the variable name.
      if (is_array($token) && $token[0] == T_VARIABLE && ($variable_name = substr($token[1], 1)) && in_array($variable_name, $keys)) {
        // Write the new value to settings.php in the following format:
        //    $[setting] = '[value]'; // [comment]
        $setting = $settings[$variable_name];
        $buffer .= '$' . $variable_name . ' = ' . var_export($setting['value'], TRUE) . ';';
        if (!empty($setting['comment'])) {
          $buffer .= ' // ' . $setting['comment'];
        }
        unset($settings[$variable_name]);
        $replacing_variable = TRUE;
      }
      else {
        // Write a regular token (that is not part of a variable we're
        // replacing) to settings.php directly.
        if (!$replacing_variable) {
          $buffer .= is_array($token) ? $token[1] : $token;
        }
        // When we hit a semicolon, we are done with the code that defines the
        // variable that is being replaced.
        if ($token == ';') {
          $replacing_variable = FALSE;
        }
      }
    }

    // Add required settings that were missing from settings.php.
    foreach ($settings as $setting => $data) {
      if (!empty($data['required'])) {
        $buffer .= "\$$setting = " . var_export($data['value'], TRUE) . ";\n";
      }
    }

    // Write the new settings file.
    if (file_put_contents(DRUPAL_ROOT . '/' . $settings_file, $buffer) === FALSE) {
      throw new Exception(st('Failed to modify %settings. Verify the file permissions.', array('%settings' => $settings_file)));
    }
  }
  else {
    throw new Exception(st('Failed to open %settings. Verify the file permissions.', array('%settings' => $settings_file)));
  }
}

/**
 * Creates the config directory and ensures it is operational.
 *
 * @see install_settings_form_submit()
 * @see update_prepare_d8_bootstrap()
 */
function drupal_install_config_directories() {
  global $config_directories;

  // Add a randomized config directory name to settings.php, unless it was
  // manually defined in the existing already.
  if (empty($config_directories)) {
    $config_directories_hash = drupal_hash_base64(drupal_random_bytes(55));
    $settings['config_directories'] = array(
      'value' => array(
        CONFIG_ACTIVE_DIRECTORY => array(
          'path' => 'config_' . $config_directories_hash . '/active',
        ),
        CONFIG_STAGING_DIRECTORY => array(
          'path' => 'config_' . $config_directories_hash . '/staging',
        ),
      ),
      'required' => TRUE,
    );
    // Rewrite settings.php, which also sets the value as global variable.
    drupal_rewrite_settings($settings);
  }

  // Ensure the config directories exist or can be created, and are writable.
  foreach (array(CONFIG_ACTIVE_DIRECTORY, CONFIG_STAGING_DIRECTORY) as $config_type) {
    // This should never fail, since if the config directory was specified in
    // settings.php it will have already been created and verified earlier, and
    // if it wasn't specified in settings.php, it is created here inside the
    // public files directory, which has already been verified to be writable
    // itself. But if it somehow fails anyway, the installation cannot proceed.
    // Bail out using a similar error message as in system_requirements().
    if (!install_ensure_config_directory($config_type)) {
      throw new Exception(st('The directory %directory could not be created or could not be made writable. To proceed with the installation, either create the directory and modify its permissions manually or ensure that the installer has the permissions to create it automatically. For more information, see the <a href="@handbook_url">online handbook</a>.', array(
        '%directory' => config_get_config_directory($config_type),
        '@handbook_url' => 'http://drupal.org/server-permissions',
      )));
    }

    // Put a README.txt into each config directory. This is required so that
    // they can later be added to git. Since these directories are auto-
    // created, we have to write out the README rather than just adding it
    // to the drupal core repo.
    switch ($config_type) {
      case CONFIG_ACTIVE_DIRECTORY:
        $text = 'This directory contains the active configuration for your Drupal site. To move this configuration between environments, contents from this directory should be placed in the staging directory on the target server. To make this configuration active, see admin/config/development/sync on the target server.';
        break;
      case CONFIG_STAGING_DIRECTORY:
        $text = 'This directory contains configuration to be imported into your Drupal site. To make this configuration active, see admin/config/development/sync.';
        break;
    }
    $text .=  ' For information about deploying configuration between servers, see http://drupal.org/documentation/administer/config';
    file_put_contents(config_get_config_directory($config_type) . '/README.txt', $text);
  }
}

/**
 * Checks whether a config directory exists and is writable.
 *
 * This partially duplicates install_ensure_config_directory(), but is required
 * since the installer would create the config directory too early in the
 * installation process otherwise (e.g., when only visiting install.php when
 * there is a settings.php already, but not actually executing the
 * installation).
 *
 * @param string $type
 *   Type of config directory to return. Drupal core provides 'active' and
 *   'staging'.
 *
 * @return bool
 *   TRUE if the config directory exists and is writable.
 */
function install_verify_config_directory($type) {
  global $config_directories;
  if (!isset($config_directories[$type])) {
    return FALSE;
  }
  // config_get_config_directory() throws an exception when the passed $type
  // does not exist in $config_directories. This can happen if there is a
  // prepared settings.php that defines $config_directories already.
  try {
    $config_directory = config_get_config_directory($type);
    if (is_dir($config_directory) && is_writable($config_directory)) {
      return TRUE;
    }
  }
  catch (\Exception $e) {
  }
  return FALSE;
}

/**
 * Ensures that the config directory exists and is writable, or can be made so.
 *
 * @param string $type
 *   Type of config directory to return. Drupal core provides 'active' and
 *   'staging'.
 *
 * @return bool
 *   TRUE if the config directory exists and is writable.
 */
function install_ensure_config_directory($type) {
  // The config directory must be defined in settings.php.
  global $config_directories;
  if (!isset($config_directories[$type])) {
    return FALSE;
  }
  // The logic here is similar to that used by system_requirements() for other
  // directories that the installer creates.
  else {
    $config_directory = config_get_config_directory($type);
    return file_prepare_directory($config_directory, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS);
  }
}

/**
 * Verifies that all dependencies are met for a given installation profile.
 *
 * @param $install_state
 *   An array of information about the current installation state.
 *
 * @return
 *   The list of modules to install.
 */
function drupal_verify_profile($install_state) {
  include_once DRUPAL_ROOT . '/core/includes/file.inc';
  include_once DRUPAL_ROOT . '/core/includes/common.inc';

  $profile = $install_state['parameters']['profile'];
  $profile_file = $install_state['profiles'][$profile]->uri;

  if (!isset($profile) || !file_exists($profile_file)) {
    throw new Exception(install_no_profile_error());
  }
  $info = $install_state['profile_info'];

  // Get a list of modules that exist in Drupal's assorted subdirectories.
  $present_modules = array();
  foreach (drupal_system_listing('/^' . DRUPAL_PHP_FUNCTION_PATTERN . '\.module$/', 'modules') as $present_module) {
    $present_modules[] = $present_module->name;
  }

  // The installation profile is also a module, which needs to be installed
  // after all the other dependencies have been installed.
  $present_modules[] = drupal_get_profile();

  // Verify that all of the profile's required modules are present.
  $missing_modules = array_diff($info['dependencies'], $present_modules);

  $requirements = array();

  if (count($missing_modules)) {
    $modules = array();
    foreach ($missing_modules as $module) {
      $modules[] = '<span class="admin-missing">' . drupal_ucfirst($module) . '</span>';
    }
    $requirements['required_modules'] = array(
      'title'       => st('Required modules'),
      'value'       => st('Required modules not found.'),
      'severity'    => REQUIREMENT_ERROR,
      'description' => st('The following modules are required but were not found. Move them into the appropriate modules subdirectory, such as <em>/modules</em>. Missing modules: !modules', array('!modules' => implode(', ', $modules))),
    );
  }
  return $requirements;
}

/**
 * Installs the system module.
 *
 * Separated from the installation of other modules so core system
 * functions can be made available while other modules are installed.
 */
function drupal_install_system() {
  // Create tables.
  drupal_install_schema('system');
  // Immediately boot a kernel to have real services ready.
  $kernel = new DrupalKernel('install', FALSE, drupal_classloader(), FALSE);
  $kernel->boot();

  $system_path = drupal_get_path('module', 'system');
  require_once DRUPAL_ROOT . '/' . $system_path . '/system.install';
  $system_versions = drupal_get_schema_versions('system');
  $system_version = $system_versions ? max($system_versions) : SCHEMA_INSTALLED;
  drupal_container()
    ->get('keyvalue')
    ->get('system.schema')
    ->set('system', $system_version);

  // System module needs to be enabled and the system/module lists need to be
  // reset first in order to allow config_install_default_config() to invoke
  // config import callbacks.
  // @todo Installation profiles may override the system.module config object.
  config('system.module')
    ->set('enabled.system', 0)
    ->save();

  // Update the module list to include it.
  drupal_container()->get('module_handler')->setModuleList(array('system' => $system_path . '/system.module'));

  config_install_default_config('module', 'system');

  module_invoke('system', 'install');
}

/**
 * Verifies the state of the specified file.
 *
 * @param $file
 *   The file to check for.
 * @param $mask
 *   An optional bitmask created from various FILE_* constants.
 * @param $type
 *   The type of file. Can be file (default), dir, or link.
 *
 * @return
 *   TRUE on success or FALSE on failure. A message is set for the latter.
 */
function drupal_verify_install_file($file, $mask = NULL, $type = 'file') {
  $return = TRUE;
  // Check for files that shouldn't be there.
  if (isset($mask) && ($mask & FILE_NOT_EXIST) && file_exists($file)) {
    return FALSE;
  }
  // Verify that the file is the type of file it is supposed to be.
  if (isset($type) && file_exists($file)) {
    $check = 'is_' . $type;
    if (!function_exists($check) || !$check($file)) {
      $return = FALSE;
    }
  }

  // Verify file permissions.
  if (isset($mask)) {
    $masks = array(FILE_EXIST, FILE_READABLE, FILE_WRITABLE, FILE_EXECUTABLE, FILE_NOT_READABLE, FILE_NOT_WRITABLE, FILE_NOT_EXECUTABLE);
    foreach ($masks as $current_mask) {
      if ($mask & $current_mask) {
        switch ($current_mask) {
          case FILE_EXIST:
            if (!file_exists($file)) {
              if ($type == 'dir') {
                drupal_install_mkdir($file, $mask);
              }
              if (!file_exists($file)) {
                $return = FALSE;
              }
            }
            break;
          case FILE_READABLE:
            if (!is_readable($file) && !drupal_install_fix_file($file, $mask)) {
              $return = FALSE;
            }
            break;
          case FILE_WRITABLE:
            if (!is_writable($file) && !drupal_install_fix_file($file, $mask)) {
              $return = FALSE;
            }
            break;
          case FILE_EXECUTABLE:
            if (!is_executable($file) && !drupal_install_fix_file($file, $mask)) {
              $return = FALSE;
            }
            break;
          case FILE_NOT_READABLE:
            if (is_readable($file) && !drupal_install_fix_file($file, $mask)) {
              $return = FALSE;
            }
            break;
          case FILE_NOT_WRITABLE:
            if (is_writable($file) && !drupal_install_fix_file($file, $mask)) {
              $return = FALSE;
            }
            break;
          case FILE_NOT_EXECUTABLE:
            if (is_executable($file) && !drupal_install_fix_file($file, $mask)) {
              $return = FALSE;
            }
            break;
        }
      }
    }
  }
  return $return;
}

/**
 * Creates a directory with the specified permissions.
 *
 * @param $file
 *  The name of the directory to create;
 * @param $mask
 *  The permissions of the directory to create.
 * @param $message
 *  (optional) Whether to output messages. Defaults to TRUE.
 *
 * @return
 *  TRUE/FALSE whether or not the directory was successfully created.
 */
function drupal_install_mkdir($file, $mask, $message = TRUE) {
  $mod = 0;
  $masks = array(FILE_READABLE, FILE_WRITABLE, FILE_EXECUTABLE, FILE_NOT_READABLE, FILE_NOT_WRITABLE, FILE_NOT_EXECUTABLE);
  foreach ($masks as $m) {
    if ($mask & $m) {
      switch ($m) {
        case FILE_READABLE:
          $mod |= 0444;
          break;
        case FILE_WRITABLE:
          $mod |= 0222;
          break;
        case FILE_EXECUTABLE:
          $mod |= 0111;
          break;
      }
    }
  }

  if (@drupal_mkdir($file, $mod)) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

/**
 * Attempts to fix file permissions.
 *
 * The general approach here is that, because we do not know the security
 * setup of the webserver, we apply our permission changes to all three
 * digits of the file permission (i.e. user, group and all).
 *
 * To ensure that the values behave as expected (and numbers don't carry
 * from one digit to the next) we do the calculation on the octal value
 * using bitwise operations. This lets us remove, for example, 0222 from
 * 0700 and get the correct value of 0500.
 *
 * @param $file
 *  The name of the file with permissions to fix.
 * @param $mask
 *  The desired permissions for the file.
 * @param $message
 *  (optional) Whether to output messages. Defaults to TRUE.
 *
 * @return
 *  TRUE/FALSE whether or not we were able to fix the file's permissions.
 */
function drupal_install_fix_file($file, $mask, $message = TRUE) {
  // If $file does not exist, fileperms() issues a PHP warning.
  if (!file_exists($file)) {
    return FALSE;
  }

  $mod = fileperms($file) & 0777;
  $masks = array(FILE_READABLE, FILE_WRITABLE, FILE_EXECUTABLE, FILE_NOT_READABLE, FILE_NOT_WRITABLE, FILE_NOT_EXECUTABLE);

  // FILE_READABLE, FILE_WRITABLE, and FILE_EXECUTABLE permission strings
  // can theoretically be 0400, 0200, and 0100 respectively, but to be safe
  // we set all three access types in case the administrator intends to
  // change the owner of settings.php after installation.
  foreach ($masks as $m) {
    if ($mask & $m) {
      switch ($m) {
        case FILE_READABLE:
          if (!is_readable($file)) {
            $mod |= 0444;
          }
          break;
        case FILE_WRITABLE:
          if (!is_writable($file)) {
            $mod |= 0222;
          }
          break;
        case FILE_EXECUTABLE:
          if (!is_executable($file)) {
            $mod |= 0111;
          }
          break;
        case FILE_NOT_READABLE:
          if (is_readable($file)) {
            $mod &= ~0444;
          }
          break;
        case FILE_NOT_WRITABLE:
          if (is_writable($file)) {
            $mod &= ~0222;
          }
          break;
        case FILE_NOT_EXECUTABLE:
          if (is_executable($file)) {
            $mod &= ~0111;
          }
          break;
      }
    }
  }

  // chmod() will work if the web server is running as owner of the file.
  // If PHP safe_mode is enabled the currently executing script must also
  // have the same owner.
  if (@chmod($file, $mod)) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

/**
 * Sends the user to a different installer page.
 *
 * This issues an on-site HTTP redirect. Messages (and errors) are erased.
 *
 * @param $path
 *   An installer path.
 */
function install_goto($path) {
  global $base_url;
  include_once DRUPAL_ROOT . '/core/includes/common.inc';
  header('Location: ' . $base_url . '/' . $path);
  header('Cache-Control: no-cache'); // Not a permanent redirect.
  drupal_exit();
}

/**
 * Returns the URL of the current script, with modified query parameters.
 *
 * This function can be called by low-level scripts (such as install.php and
 * update.php) and returns the URL of the current script. Existing query
 * parameters are preserved by default, but new ones can optionally be merged
 * in.
 *
 * This function is used when the script must maintain certain query parameters
 * over multiple page requests in order to work correctly. In such cases (for
 * example, update.php, which requires the 'continue=1' parameter to remain in
 * the URL throughout the update process if there are any requirement warnings
 * that need to be bypassed), using this function to generate the URL for links
 * to the next steps of the script ensures that the links will work correctly.
 *
 * @param $query
 *   (optional) An array of query parameters to merge in to the existing ones.
 *
 * @return
 *   The URL of the current script, with query parameters modified by the
 *   passed-in $query. The URL is not sanitized, so it still needs to be run
 *   through check_url() if it will be used as an HTML attribute value.
 *
 * @see drupal_requirements_url()
 */
function drupal_current_script_url($query = array()) {
  $uri = $_SERVER['SCRIPT_NAME'];
  $query = array_merge(drupal_get_query_parameters(), $query);
  if (!empty($query)) {
    $uri .= '?' . drupal_http_build_query($query);
  }
  return $uri;
}

/**
 * Returns a URL for proceeding to the next page after a requirements problem.
 *
 * This function can be called by low-level scripts (such as install.php and
 * update.php) and returns a URL that can be used to attempt to proceed to the
 * next step of the script.
 *
 * @param $severity
 *   The severity of the requirements problem, as returned by
 *   drupal_requirements_severity().
 *
 * @return
 *   A URL for attempting to proceed to the next step of the script. The URL is
 *   not sanitized, so it still needs to be run through check_url() if it will
 *   be used as an HTML attribute value.
 *
 * @see drupal_current_script_url()
 */
function drupal_requirements_url($severity) {
  $query = array();
  // If there are no errors, only warnings, append 'continue=1' to the URL so
  // the user can bypass this screen on the next page load.
  if ($severity == REQUIREMENT_WARNING) {
    $query['continue'] = 1;
  }
  return drupal_current_script_url($query);
}

/**
 * Translates a string when some systems are not available.
 *
 * Used during the install process, when database, theme, and localization
 * system is possibly not yet available.
 *
 * Use t() if your code will never run during the Drupal installation phase.
 * Use st() if your code will only run during installation and never any other
 * time. Use get_t() if your code could run in either circumstance.
 *
 * @see t()
 * @see get_t()
 * @ingroup sanitization
 */
function st($string, array $args = array(), array $options = array()) {
  static $strings = NULL;
  global $install_state;

  if (empty($options['context'])) {
    $options['context'] = '';
  }

  if (!isset($strings)) {
    $strings = array();
    if (isset($install_state['parameters']['langcode'])) {
      // If the given langcode was selected, there should be at least one .po
      // file with its name in the pattern drupal-$version.$langcode.po.
      // This might or might not be the entire filename. It is also possible
      // that multiple files end with the same suffix, even if unlikely.
      $files = install_find_translation_files($install_state['parameters']['langcode']);
      if (!empty($files)) {
        // Register locale classes with the classloader. Locale module is not
        // yet enabled at this stage, so this is not happening automatically.
        drupal_classloader_register('locale', drupal_get_path('module', 'locale'));
        $strings = Gettext::filesToArray($install_state['parameters']['langcode'], $files);
      }
    }
  }

  require_once DRUPAL_ROOT . '/core/includes/theme.inc';
  // Transform arguments before inserting them
  foreach ($args as $key => $value) {
    switch ($key[0]) {
      // Escaped only
      case '@':
        $args[$key] = check_plain($value);
        break;
      // Escaped and placeholder
      case '%':
      default:
        $args[$key] = '<em>' . check_plain($value) . '</em>';
        break;
      // Pass-through
      case '!':
    }
  }
  return strtr((!empty($strings[$options['context']][$string]) ? $strings[$options['context']][$string] : $string), $args);
}

/**
 * Checks an installation profile's requirements.
 *
 * @param string $profile
 *   Name of installation profile to check.
 * @param array $install_state
 *   The current state in the install process.
 *
 * @return array
 *   Array of the installation profile's requirements.
 */
function drupal_check_profile($profile, array $install_state) {
  include_once DRUPAL_ROOT . '/core/includes/file.inc';

  $profile_file = $install_state['profiles'][$profile]->uri;

  if (!isset($profile) || !file_exists($profile_file)) {
    throw new Exception(install_no_profile_error());
  }

  $info = install_profile_info($profile);

  // Collect requirement testing results.
  $requirements = array();
  foreach ($info['dependencies'] as $module) {
    module_load_install($module);
    $function = $module . '_requirements';
    if (function_exists($function)) {
      $requirements = array_merge($requirements, $function('install'));
    }
  }
  return $requirements;
}

/**
 * Extracts the highest severity from the requirements array.
 *
 * @param $requirements
 *   An array of requirements, in the same format as is returned by
 *   hook_requirements().
 *
 * @return
 *   The highest severity in the array.
 */
function drupal_requirements_severity(&$requirements) {
  $severity = REQUIREMENT_OK;
  foreach ($requirements as $requirement) {
    if (isset($requirement['severity'])) {
      $severity = max($severity, $requirement['severity']);
    }
  }
  return $severity;
}

/**
 * Checks a module's requirements.
 *
 * @param $module
 *   Machine name of module to check.
 *
 * @return
 *   TRUE or FALSE, depending on whether the requirements are met.
 */
function drupal_check_module($module) {
  module_load_install($module);
  if (module_hook($module, 'requirements')) {
    // Check requirements
    $requirements = module_invoke($module, 'requirements', 'install');
    if (is_array($requirements) && drupal_requirements_severity($requirements) == REQUIREMENT_ERROR) {
      // Print any error messages
      foreach ($requirements as $requirement) {
        if (isset($requirement['severity']) && $requirement['severity'] == REQUIREMENT_ERROR) {
          $message = $requirement['description'];
          if (isset($requirement['value']) && $requirement['value']) {
            $message .= ' (' . t('Currently using !item !version', array('!item' => $requirement['title'], '!version' => $requirement['value'])) . ')';
          }
          drupal_set_message($message, 'error');
        }
      }
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Retrieves information about an installation profile from its .info file.
 *
 * The information stored in a profile .info file is similar to that stored in
 * a normal Drupal module .info file. For example:
 * - name: The real name of the installation profile for display purposes.
 * - description: A brief description of the profile.
 * - dependencies: An array of shortnames of other modules that this install
 *   profile requires.
 *
 * Additional, less commonly-used information that can appear in a profile.info
 * file but not in a normal Drupal module .info file includes:
 * - distribution_name: The name of the Drupal distribution that is being
 *   installed, to be shown throughout the installation process. Defaults to
 *   'Drupal'.
 * - exclusive: If the install profile is intended to be the only eligible
 *   choice in a distribution, setting exclusive = TRUE will auto-select it
 *   during installation, and the install profile selection screen will be
 *   skipped. If more than one profile is found where exclusive = TRUE then
 *   this property will have no effect and the profile selection screen will
 *   be shown as normal with all available profiles shown.
 *
 * Note that this function does an expensive file system scan to get info file
 * information for dependencies. If you only need information from the info
 * file itself, use system_get_info().
 *
 * Example of .info file:
 * @code
 *    name = Minimal
 *    description = Start fresh, with only a few modules enabled.
 *    dependencies[] = block
 *    dependencies[] = dblog
 * @endcode
 *
 * @param $profile
 *   Name of profile.
 * @param $langcode
 *   Language code (if any).
 *
 * @return
 *   The info array.
 */
function install_profile_info($profile, $langcode = 'en') {
  $cache = &drupal_static(__FUNCTION__, array());

  if (!isset($cache[$profile])) {
    // Set defaults for module info.
    $defaults = array(
      'dependencies' => array(),
      'description' => '',
      'distribution_name' => 'Drupal',
      'version' => NULL,
      'hidden' => FALSE,
      'php' => DRUPAL_MINIMUM_PHP,
    );
    $profile_file = drupal_get_path('profile', $profile) . "/$profile.info";
    $info = drupal_parse_info_file($profile_file);
    $info += $defaults;
    $info['dependencies'] = array_unique(array_merge(
      drupal_required_modules(),
      $info['dependencies'],
      ($langcode != 'en' && !empty($langcode) ? array('locale') : array()))
    );

    // drupal_required_modules() includes the current profile as a dependency.
    // Since a module can't depend on itself we remove that element of the array.
    array_shift($info['dependencies']);

    $cache[$profile] = $info;
  }
  return $cache[$profile];
}

/**
 * Ensures the environment for a Drupal database on a predefined connection.
 *
 * This will run tasks that check that Drupal can perform all of the functions
 * on a database, that Drupal needs. Tasks include simple checks like CREATE
 * TABLE to database specific functions like stored procedures and client
 * encoding.
 */
function db_run_tasks($driver) {
  db_installer_object($driver)->runTasks();
  return TRUE;
}

/**
 * Returns a database installer object.
 *
 * @param $driver
 *   The name of the driver.
 */
function db_installer_object($driver) {
  // We cannot use Database::getConnection->getDriverClass() here, because
  // the connection object is not yet functional.
  $task_class = "Drupal\\Core\\Database\\Driver\\{$driver}\\Install\\Tasks";
  if (class_exists($task_class)) {
    return new $task_class();
  }
  else {
    $task_class = "Drupal\\Driver\\Database\\{$driver}\\Install\\Tasks";
    return new $task_class();
  }
}
