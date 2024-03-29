<?php

use Drupal\Core\Config\Config;
use Drupal\Core\Config\ConfigException;
use Drupal\Core\Config\FileStorage;
use Drupal\Core\Config\StorageInterface;

/**
 * @file
 * This is the API for configuration storage.
 */

/**
 * Config import lock name used to prevent concurrent synchronizations.
 */
const CONFIG_IMPORT_LOCK = 'config_import';

/**
 * Installs the default configuration of a given extension.
 *
 * @param string $type
 *   The extension type; e.g., 'module' or 'theme'.
 * @param string $name
 *   The name of the module or theme to install default configuration for.
 */
function config_install_default_config($type, $name) {
  // Use the override free context for config importing so that any overrides do
  // not change the data on import.
  config_context_enter('config.context.free');

  // If this module defines any ConfigEntity types then create an empty
  // manifest file for each of them.
  foreach (config_get_module_config_entities($name) as $entity_info) {
    config('manifest.' . $entity_info['config_prefix'])->save();
  }

  $config_dir = drupal_get_path($type, $name) . '/config';
  if (is_dir($config_dir)) {
    $source_storage = new FileStorage($config_dir);
    $target_storage = drupal_container()->get('config.storage');

    // Ignore manifest files.
    $config_changes = config_sync_get_changes($source_storage, $target_storage, FALSE);
    if (empty($config_changes['create'])) {
      return;
    }

    // Do not overwrite or delete pre-existing configuration.
    $config_changes['change'] = array();
    $config_changes['delete'] = array();
    $remaining_changes = config_import_invoke_owner($config_changes, $source_storage, $target_storage);
    config_sync_changes($remaining_changes, $source_storage, $target_storage);
  }
  // Exit the override free context.
  config_context_leave();
}

/**
 * Uninstalls the default configuration of a given extension.
 *
 * @param string $type
 *   The extension type; e.g., 'module' or 'theme'.
 * @param string $name
 *   The name of the module or theme to install default configuration for.
 */
function config_uninstall_default_config($type, $name) {
  $storage = drupal_container()->get('config.storage');
  $config_names = $storage->listAll($name . '.');
  foreach ($config_names as $config_name) {
    config($config_name)->delete();
  }

  // If this module defines any ConfigEntity types, then delete the manifest
  // file for each of them.
  foreach (config_get_module_config_entities($name) as $entity_type) {
    config('manifest.' . $entity_info['config_prefix'])->delete();
  }
}

/**
 * Gets configuration object names starting with a given prefix.
 *
 * @see Drupal\Core\Config\StorageInterface::listAll()
 */
function config_get_storage_names_with_prefix($prefix = '') {
  return drupal_container()->get('config.storage')->listAll($prefix);
}

/**
 * Retrieves a configuration object.
 *
 * This is the main entry point to the configuration API. Calling
 * @code config('book.admin') @endcode will return a configuration object in
 * which the book module can store its administrative settings.
 *
 * @param string $name
 *   The name of the configuration object to retrieve. The name corresponds to
 *   a configuration file. For @code config('book.admin') @endcode, the config
 *   object returned will contain the contents of book.admin configuration file.
 *
 * @return Drupal\Core\Config\Config
 *   A configuration object.
 */
function config($name) {
  return drupal_container()->get('config.factory')->get($name);
}

/*
 * Sets the config context on the config factory.
 *
 * This allows configuration objects to be created using special configuration
 * contexts eg. global override free or locale using a user preferred language.
 * Calling this function affects all subsequent calls to config() until
 * config_context_leave() is called.
 *
 * @see config_context_leave()
 * @see \Drupal\Core\Config\ConfigFactory
 *
 * @param string $context_name
 *   The name of the config context service on the container or a fully
 *   qualified class implementing \Drupal\Core\Config\Context\ContextInterface.
 *
 * @return \Drupal\Core\Config\Context\ContextInterface
 *   The configuration context object.
 */
function config_context_enter($context_name) {
  if (drupal_container()->has($context_name)) {
    $context = drupal_container()->get($context_name);
  }
  elseif (class_exists($context_name) && in_array("Drupal\\Core\\Config\\Context\\ContextInterface", class_implements($context_name))) {
    $context = drupal_container()
      ->get('config.context.factory')
      ->get($context_name);
  }
  else {
    throw new ConfigException(sprintf('Unknown config context service or class: %s', $context_name));
  }
  drupal_container()
    ->get('config.factory')
    ->enterContext($context);
  return $context;
}

/*
 * Leaves the current config context returning to the previous context.
 *
 * @see config_context_enter()
 * @see \Drupal\Core\Config\ConfigFactory
 */
function config_context_leave() {
  drupal_container()
    ->get('config.factory')
    ->leaveContext();
}

/**
 * Returns a list of differences between configuration storages.
 *
 * @param Drupal\Core\Config\StorageInterface $source_storage
 *   The storage to synchronize configuration from.
 * @param Drupal\Core\Config\StorageInterface $target_storage
 *   The storage to synchronize configuration to.
 * @param bool $use_manifest
 *   (optional) Whether to determine changes based on manifest files. Defaults
 *   to TRUE.
 *
 * @return array|bool
 *   An associative array containing the differences between source and target
 *   storage, or FALSE if there are no differences.
 */
function config_sync_get_changes(StorageInterface $source_storage, StorageInterface $target_storage, $use_manifest = TRUE) {
  $config_changes = array(
    'create' => array(),
    'change' => array(),
    'delete' => array(),
  );
  $all_source_names = $source_storage->listAll();
  $all_target_names = $target_storage->listAll();

  // Config entities maintain 'manifest' files that list the objects they
  // are currently handling. Each file is a simple indexed array of config
  // object names. In order to generate a list of objects that have been
  // created or deleted we need to open these files in both the source and
  // target storage, generate an array of the objects, and compare them.
  if ($use_manifest) {
    $source_config_data = array();
    $target_config_data = array();
    foreach ($source_storage->listAll('manifest') as $name) {
      if ($source_manifest_data = $source_storage->read($name)) {
        $source_config_data = array_merge($source_config_data, $source_manifest_data);
      }

      if ($target_manifest_data = $target_storage->read($name)) {
        $target_config_data = array_merge($target_config_data, $target_manifest_data);
      }
    }

    foreach (array_diff_key($target_config_data, $source_config_data) as $name => $value) {
      $config_changes['delete'][] = $value['name'];
    }

    foreach (array_diff_key($source_config_data, $target_config_data) as $name => $value) {
      $config_changes['create'][] = $value['name'];
    }
  }
  else {
    $config_changes['delete'] = array_diff($all_target_names, $all_source_names);
    $config_changes['create'] = array_diff($all_source_names, $all_target_names);
  }

  foreach (array_intersect($all_source_names, $all_target_names) as $name) {
    // Ignore manifest files
    if (substr($name, 0, 9) != 'manifest.') {
      $source_config_data = $source_storage->read($name);
      $target_config_data = $target_storage->read($name);
      if ($source_config_data !== $target_config_data) {
        $config_changes['change'][] = $name;
      }
    }
  }

  // Do not trigger subsequent synchronization operations if there are no
  // changes in any category.
  if (empty($config_changes['create']) && empty($config_changes['change']) && empty($config_changes['delete'])) {
    return FALSE;
  }
  return $config_changes;
}

/**
 * Writes an array of config file changes from a source storage to a target storage.
 *
 * @param array $config_changes
 *   An array of changes to be written.
 * @param Drupal\Core\Config\StorageInterface $source_storage
 *   The storage to synchronize configuration from.
 * @param Drupal\Core\Config\StorageInterface $target_storage
 *   The storage to synchronize configuration to.
 */
function config_sync_changes(array $config_changes, StorageInterface $source_storage, StorageInterface $target_storage) {
  $target_context = drupal_container()->get('config.context.free');
  $factory = drupal_container()->get('config.factory');
  foreach (array('delete', 'create', 'change') as $op) {
    foreach ($config_changes[$op] as $name) {
      $config = new Config($name, $target_storage, $target_context);
      if ($op == 'delete') {
        $config->delete();
      }
      else {
        $data = $source_storage->read($name);
        $config->setData($data ? $data : array());
        $config->save();
      }
      $factory->reset($name);
    }
  }
}

/**
 * Imports configuration into the active configuration.
 *
 * @return bool|null
 *   TRUE if configuration was imported successfully, FALSE in case of a
 *   synchronization error, or NULL if there are no changes to synchronize.
 */
function config_import() {
  // Retrieve a list of differences between staging and the active configuration.
  $source_storage = drupal_container()->get('config.storage.staging');
  $snapshot_storage = drupal_container()->get('config.storage.snapshot');
  $target_storage = drupal_container()->get('config.storage');

  $config_changes = config_sync_get_changes($source_storage, $target_storage);
  if (empty($config_changes)) {
    return;
  }

  if (!lock()->acquire(CONFIG_IMPORT_LOCK)) {
    // Another request is synchronizing configuration.
    // Return a negative result for UI purposes. We do not differentiate between
    // an actual synchronization error and a failed lock, because concurrent
    // synchronizations are an edge-case happening only when multiple developers
    // or site builders attempt to do it without coordinating.
    return FALSE;
  }

  $success = TRUE;
  try {
    // Use the override free context for config importing so that any overrides do
    // not change the data on import.
    config_context_enter('config.context.free');

    $remaining_changes = config_import_invoke_owner($config_changes, $source_storage, $target_storage);
    config_sync_changes($remaining_changes, $source_storage, $target_storage);
    config_import_create_snapshot($target_storage, $snapshot_storage);

    // Exit the override free context.
    config_context_leave();
  }
  catch (ConfigException $e) {
    watchdog_exception('config_import', $e);
    $success = FALSE;
  }
  lock()->release(CONFIG_IMPORT_LOCK);

  return $success;
}

/**
 * Creates a configuration snapshot following a successful import.
 *
 * @param Drupal\Core\Config\StorageInterface $source_storage
 *   The storage to synchronize configuration from.
 * @param Drupal\Core\Config\StorageInterface $target_storage
 *   The storage to synchronize configuration to.
 */
function config_import_create_snapshot(StorageInterface $source_storage, StorageInterface $snapshot_storage) {
  $snapshot_storage->deleteAll();
  foreach ($source_storage->listAll() as $name) {
    $snapshot_storage->write($name, $source_storage->read($name));
  }
}

/**
 * Invokes MODULE_config_import() callbacks for configuration changes.
 *
 * @param array $config_changes
 *   An array of changes to be loaded.
 * @param Drupal\Core\Config\StorageInterface $source_storage
 *   The storage to synchronize configuration from.
 * @param Drupal\Core\Config\StorageInterface $target_storage
 *   The storage to synchronize configuration to.
 *
 * @todo Add support for other extension types; e.g., themes etc.
 */
function config_import_invoke_owner(array $config_changes, StorageInterface $source_storage, StorageInterface $target_storage) {
  $factory = drupal_container()->get('config.factory');
  // Use the admin context for config importing so that any overrides do not
  // change the data on import.
  $free_context = drupal_container()->get('config.context.free');
  // Allow modules to take over configuration change operations for
  // higher-level configuration data.
  // First pass deleted, then new, and lastly changed configuration, in order to
  // handle dependencies correctly.
  $manager = drupal_container()->get('plugin.manager.entity');
  foreach (array('delete', 'create', 'change') as $op) {
    foreach ($config_changes[$op] as $key => $name) {
      // Call to the configuration entity's storage controller to handle the
      // configuration change.
      $handled_by_module = FALSE;
      // Validate the configuration object name before importing it.
      Config::validateName($name);
      if ($entity_type = config_get_entity_type_by_name($name)) {
        $old_config = new Config($name, $target_storage, $free_context);
        $old_config->load();

        $data = $source_storage->read($name);
        $new_config = new Config($name, $source_storage, $free_context);
        if ($data !== FALSE) {
          $new_config->setData($data);
        }

        $method = 'import' . ucfirst($op);
        $handled_by_module = $manager->getStorageController($entity_type)->$method($name, $new_config, $old_config);
      }
      if (!empty($handled_by_module)) {
        $factory->reset($name);
        // Reset the manifest config object for the config entity.
        $entity_info = drupal_container()->get('plugin.manager.entity')->getDefinition($entity_type);
        $factory->reset('manifest.' . $entity_info['config_prefix']);
        unset($config_changes[$op][$key]);
      }
    }
  }
  return $config_changes;
}

/**
 * Return a list of all config entity types provided by a module.
 *
 * @param string $module
 *   The name of the module possibly providing config entities.
 *
 * @return array
 *   An associative array containing the entity info for any config entities
 *   provided by the requested module, keyed by the entity type.
 */
function config_get_module_config_entities($module) {
  // While this is a lot of work to generate, it's not worth static caching
  // since this function is only called at install/uninstall, and only
  // once per module.
  $info = entity_get_info();
  return array_filter($info, function($entity_info) use ($module) {
    return ($entity_info['module'] == $module) && is_subclass_of($entity_info['class'], 'Drupal\Core\Config\Entity\ConfigEntityInterface');
  });
}

/**
 * Returns the entity type of a configuration object.
 *
 * @param string $name
 *   The configuration object name.
 *
 * @return string|null
 *   Either the entity type name, or NULL if none match.
 */
function config_get_entity_type_by_name($name) {
  $entities = array_filter(entity_get_info(), function($entity_info) use ($name) {
    return (isset($entity_info['config_prefix']) && strpos($name, $entity_info['config_prefix'] . '.') === 0);
  });
  return key($entities);
}

/*
 * Returns the typed config manager service.
 *
 * Use the typed data manager service for creating typed configuration objects.
 *
 * @see Drupal\Core\TypedData\TypedDataManager::create()
 *
 * @return Drupal\Core\TypedData\TypedConfigManager
 */
function config_typed() {
  return drupal_container()->get('config.typed');
}
