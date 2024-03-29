<?php

/**
 * @file
 * Definition of Drupal\views\Tests\ModuleTest.
 */

namespace Drupal\views\Tests;

/**
 * Tests basic functions from the Views module.
 */
class ModuleTest extends ViewUnitTestBase {

  /**
   * Views used by this test.
   *
   * @var array
   */
  public static $testViews = array('test_view_status');

  public static function getInfo() {
    return array(
      'name' => 'Views Module tests',
      'description' => 'Tests some basic functions of views.module.',
      'group' => 'Views',
    );
  }

  /**
   * Tests the views_get_handler method.
   */
  function testviews_get_handler() {
    $types = array('field', 'area', 'filter');
    foreach ($types as $type) {
      $handler = views_get_handler($this->randomName(), $this->randomName(), $type);
      $this->assertEqual('Drupal\views\Plugin\views\\' . $type . '\Broken', get_class($handler), t('Make sure that a broken handler of type: @type are created', array('@type' => $type)));
    }

    $views_data = $this->viewsData();
    $test_tables = array('views_test_data' => array('id', 'name'));
    foreach ($test_tables as $table => $fields) {
      foreach ($fields as $field) {
        $data = $views_data[$table][$field];
        foreach ($data as $id => $field_data) {
          if (!in_array($id, array('title', 'help'))) {
            $handler = views_get_handler($table, $field, $id);
            $this->assertInstanceHandler($handler, $table, $field, $id);
          }
        }
      }
    }

    // Test the override handler feature.
    $handler = views_get_handler('views_test_data', 'job', 'filter', 'standard');
    $this->assertEqual('Drupal\\views\\Plugin\\views\\filter\\Standard', get_class($handler));
  }

  /**
   * Tests the load wrapper/helper functions.
   */
  public function testLoadFunctions() {
    $this->enableModules(array('node'));
    $controller = $this->container->get('plugin.manager.entity')->getStorageController('view');

    // Test views_view_is_enabled/disabled.
    $load = $controller->load(array('archive'));
    $archive = reset($load);
    $this->assertTrue(views_view_is_disabled($archive), 'views_view_is_disabled works as expected.');
    // Enable the view and check this.
    $archive->enable();
    $this->assertTrue(views_view_is_enabled($archive), ' views_view_is_enabled works as expected.');

    // We can store this now, as we have enabled/disabled above.
    $all_views = $controller->load();

    // Test views_get_all_views().
    $this->assertIdentical(array_keys($all_views), array_keys(views_get_all_views()), 'views_get_all_views works as expected.');

    // Test views_get_enabled_views().
    $expected_enabled = array_filter($all_views, function($view) {
      return views_view_is_enabled($view);
    });
    $this->assertIdentical(array_keys($expected_enabled), array_keys(views_get_enabled_views()), 'Expected enabled views returned.');

    // Test views_get_disabled_views().
    $expected_disabled = array_filter($all_views, function($view) {
      return views_view_is_disabled($view);
    });
    $this->assertIdentical(array_keys($expected_disabled), array_keys(views_get_disabled_views()), 'Expected disabled views returned.');

    // Test views_get_views_as_options().
    // Test the $views_only parameter.
    $this->assertIdentical(array_keys($all_views), array_keys(views_get_views_as_options(TRUE)), 'Expected option keys for all views were returned.');
    $expected_options = array();
    foreach ($all_views as $id => $view) {
      $expected_options[$id] = $view->getHumanName();
    }
    $this->assertIdentical($expected_options, views_get_views_as_options(TRUE), 'Expected options array was returned.');

    // Test the default.
    $this->assertIdentical($this->formatViewOptions($all_views), views_get_views_as_options(), 'Expected options array for all views was returned.');
    // Test enabled views.
    $this->assertIdentical($this->formatViewOptions($expected_enabled), views_get_views_as_options(FALSE, 'enabled'), 'Expected enabled options array was returned.');
    // Test disabled views.
    $this->assertIdentical($this->formatViewOptions($expected_disabled), views_get_views_as_options(FALSE, 'disabled'), 'Expected disabled options array was returned.');

    // Test the sort parameter.
    $all_views_sorted = $all_views;
    ksort($all_views_sorted);
    $this->assertIdentical(array_keys($all_views_sorted), array_keys(views_get_views_as_options(TRUE, 'all', NULL, FALSE, TRUE)), 'All view id keys returned in expected sort order');

    // Test $exclude_view parameter.
    $this->assertFalse(array_key_exists('archive', views_get_views_as_options(TRUE, 'all', 'archive')), 'View excluded from options based on name');
    $this->assertFalse(array_key_exists('archive:default', views_get_views_as_options(FALSE, 'all', 'archive:default')), 'View display excluded from options based on name');
    $this->assertFalse(array_key_exists('archive', views_get_views_as_options(TRUE, 'all', $archive->get('executable'))), 'View excluded from options based on object');

    // Test the $opt_group parameter.
    $expected_opt_groups = array();
    foreach ($all_views as $id => $view) {
      foreach ($view->get('display') as $display_id => $display) {
          $expected_opt_groups[$view->id()][$view->id() . ':' . $display['id']] = t('@view : @display', array('@view' => $view->id(), '@display' => $display['id']));
      }
    }
    $this->assertIdentical($expected_opt_groups, views_get_views_as_options(FALSE, 'all', NULL, TRUE), 'Expected option array for an option group returned.');
  }

  /**
   * Tests view enable and disable procedural wrapper functions.
   */
  function testStatusFunctions() {
    $view = views_get_view('test_view_status')->storage;

    $this->assertFalse($view->status(), 'The view status is disabled.');

    views_enable_view($view);
    $this->assertTrue($view->status(), 'A view has been enabled.');
    $this->assertEqual($view->status(), views_view_is_enabled($view), 'views_view_is_enabled is correct.');

    views_disable_view($view);
    $this->assertFalse($view->status(), 'A view has been disabled.');
    $this->assertEqual(!$view->status(), views_view_is_disabled($view), 'views_view_is_disabled is correct.');
  }

  /**
   * Helper to return an expected views option array.
   *
   * @param array $views
   *   An array of Drupal\views\Plugin\Core\Entity\View objects for which to
   *   create an options array.
   *
   * @return array
   *   A formatted options array that matches the expected output.
   */
  protected function formatViewOptions(array $views = array()) {
    $expected_options = array();
    foreach ($views as $id => $view) {
      foreach ($view->get('display') as $display_id => $display) {
        $expected_options[$view->id() . ':' . $display['id']] = t('View: @view - Display: @display',
          array('@view' => $view->id(), '@display' => $display['id']));
      }
    }

    return $expected_options;
  }

  /**
   * Ensure that a certain handler is a instance of a certain table/field.
   */
  function assertInstanceHandler($handler, $table, $field, $id) {
    $table_data = drupal_container()->get('views.views_data')->get($table);
    $field_data = $table_data[$field][$id];

    $this->assertEqual($field_data['id'], $handler->getPluginId());
  }

}
