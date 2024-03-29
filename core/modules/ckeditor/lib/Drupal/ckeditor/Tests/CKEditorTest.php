<?php

/**
 * @file
 * Definition of \Drupal\ckeditor\Tests\CKEditorTest.
 */

namespace Drupal\ckeditor\Tests;

use Drupal\simpletest\DrupalUnitTestBase;
use Drupal\editor\Plugin\EditorManager;
use Drupal\ckeditor\Plugin\editor\editor\CKEditor;

/**
 * Tests for the 'CKEditor' text editor plugin.
 */
class CKEditorTest extends DrupalUnitTestBase {

  /**
   * Modules to enable.
   *
   * @var array
   */
  public static $modules = array('system', 'editor', 'ckeditor');

  /**
   * An instance of the "CKEditor" text editor plugin.
   *
   * @var \Drupal\ckeditor\Plugin\editor\editor\CKEditor;
   */
  protected $ckeditor;

  public static function getInfo() {
    return array(
      'name' => 'CKEditor text editor plugin',
      'description' => 'Tests all aspects of the CKEditor text editor plugin.',
      'group' => 'CKEditor',
    );
  }

  function setUp() {
    parent::setUp();

    // Install the Filter module.
    $this->installSchema('system', 'url_alias');
    $this->enableModules(array('user', 'filter'));

    // Create text format, associate CKEditor.
    $filtered_html_format = entity_create('filter_format', array(
      'format' => 'filtered_html',
      'name' => 'Filtered HTML',
      'weight' => 0,
      'filters' => array(
        'filter_html' => array(
          'status' => 1,
        ),
      ),
    ));
    $filtered_html_format->save();
    $editor = entity_create('editor', array(
      'format' => 'filtered_html',
      'editor' => 'ckeditor',
    ));
    $editor->save();

    // Create "CKEditor" text editor plugin instance.
    $manager = new EditorManager($this->container->getParameter('container.namespaces'));
    $this->ckeditor = $manager->createInstance('ckeditor');
  }

  /**
   * Tests CKEditor::getJSSettings().
   */
  function testGetJSSettings() {
    $editor = entity_load('editor', 'filtered_html');

    // Default toolbar.
    $expected_config = $this->getDefaultInternalConfig() + array(
      'toolbar' => $this->getDefaultToolbarConfig(),
      'contentsCss' => $this->getDefaultContentsCssConfig(),
      'extraPlugins' => '',
      'language' => 'en',
      'drupalExternalPlugins' => array(),
    );
    $this->assertIdentical($expected_config, $this->ckeditor->getJSSettings($editor), 'Generated JS settings are correct for default configuration.');

    // Customize the configuration: add button, have two contextually enabled
    // buttons, and configure a CKEditor plugin setting.
    $this->enableModules(array('ckeditor_test'));
    drupal_container()->get('plugin.manager.ckeditor.plugin')->clearCachedDefinitions();
    $editor->settings['toolbar']['buttons'][0][] = 'Strike';
    $editor->settings['toolbar']['buttons'][1][] = 'Format';
    $editor->settings['plugins']['internal']['link_shortcut'] = 'CTRL+K';
    $editor->save();
    $expected_config['toolbar'][count($expected_config['toolbar'])-2]['items'][] = 'Strike';
    $expected_config['toolbar'][]['items'][] = 'Format';
    $expected_config['toolbar'][] = '/';
    $expected_config['format_tags'] = 'p;h4;h5;h6';
    $expected_config['extraPlugins'] = 'llama_contextual,llama_contextual_and_button';
    $expected_config['drupalExternalPlugins']['llama_contextual'] = file_create_url('core/modules/ckeditor/tests/modules/js/llama_contextual.js');
    $expected_config['drupalExternalPlugins']['llama_contextual_and_button'] = file_create_url('core/modules/ckeditor/tests/modules/js/llama_contextual_and_button.js');
    $expected_config['contentsCss'][] = file_create_url('core/modules/ckeditor/tests/modules/ckeditor_test.css');
    $expected_config['keystrokes'] = array(array(1114187, 'link'), array(1114188, NULL));
    $this->assertEqual($expected_config, $this->ckeditor->getJSSettings($editor), 'Generated JS settings are correct for customized configuration.');

    // Change the allowed HTML tags; the "format_tags" setting for CKEditor
    // should automatically be updated as well.
    $format = entity_load('filter_format', 'filtered_html');
    $format->filters['filter_html']['settings']['allowed_html'] .= '<pre> <h3>';
    $format->save();
    $expected_config['format_tags'] = 'p;h3;h4;h5;h6;pre';
    $this->assertEqual($expected_config, $this->ckeditor->getJSSettings($editor), 'Generated JS settings are correct for customized configuration.');
  }

  /**
   * Tests CKEditor::buildToolbarJSSetting().
   */
  function testBuildToolbarJSSetting() {
    $editor = entity_load('editor', 'filtered_html');

    // Default toolbar.
    $expected = $this->getDefaultToolbarConfig();
    $this->assertIdentical($expected, $this->ckeditor->buildToolbarJSSetting($editor), '"toolbar" configuration part of JS settings built correctly for default toolbar.');

    // Customize the configuration.
    $editor->settings['toolbar']['buttons'][0][] = 'Strike';
    $editor->save();
    $expected[count($expected)-2]['items'][] = 'Strike';
    $this->assertIdentical($expected, $this->ckeditor->buildToolbarJSSetting($editor), '"toolbar" configuration part of JS settings built correctly for customized toolbar.');

    // Enable the editor_test module, customize further.
    $this->enableModules(array('ckeditor_test'));
    drupal_container()->get('plugin.manager.ckeditor.plugin')->clearCachedDefinitions();
    $editor->settings['toolbar']['buttons'][0][] = 'Llama';
    $editor->save();
    $expected[count($expected)-2]['items'][] = 'Llama';
    $this->assertIdentical($expected, $this->ckeditor->buildToolbarJSSetting($editor), '"toolbar" configuration part of JS settings built correctly for customized toolbar with contrib module-provided CKEditor plugin.');
  }

  /**
   * Tests CKEditor::buildContentsCssJSSetting().
   */
  function testBuildContentsCssJSSetting() {
    $editor = entity_load('editor', 'filtered_html');

    // Default toolbar.
    $expected = $this->getDefaultContentsCssConfig();
    $this->assertIdentical($expected, $this->ckeditor->buildContentsCssJSSetting($editor), '"contentsCss" configuration part of JS settings built correctly for default toolbar.');

    // Enable the editor_test module, which implements hook_ckeditor_css_alter().
    $this->enableModules(array('ckeditor_test'));
    $expected[] = file_create_url('core/modules/ckeditor/tests/modules/ckeditor_test.css');
    $this->assertIdentical($expected, $this->ckeditor->buildContentsCssJSSetting($editor), '"contentsCss" configuration part of JS settings built correctly while a hook_ckeditor_css_alter() implementation exists.');

    // @todo test coverage for _ckeditor_theme_css(), by including a custom theme in this test with a "ckeditor_stylesheets" entry in its .info file.
  }

  /**
   * Tests Internal::getConfig().
   */
  function testInternalGetConfig() {
    $editor = entity_load('editor', 'filtered_html');
    $manager = drupal_container()->get('plugin.manager.ckeditor.plugin');
    $internal_plugin = $manager->createInstance('internal');

    // Default toolbar.
    $expected = $this->getDefaultInternalConfig();
    $this->assertIdentical($expected, $internal_plugin->getConfig($editor), '"Internal" plugin configuration built correctly for default toolbar.');

    // Format dropdown/button enabled: new setting should be present.
    $editor->settings['toolbar']['buttons'][0][] = 'Format';
    $expected['format_tags'] = 'p;h4;h5;h6';
    $this->assertIdentical($expected, $internal_plugin->getConfig($editor), '"Internal" plugin configuration built correctly for customized toolbar.');
  }

  /**
   * Tests StylesCombo::getConfig().
   */
  function testStylesComboGetConfig() {
    $editor = entity_load('editor', 'filtered_html');
    $manager = drupal_container()->get('plugin.manager.ckeditor.plugin');
    $stylescombo_plugin = $manager->createInstance('stylescombo');

    // Default toolbar.
    $expected = $this->getDefaultStylesComboConfig();
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor), '"StylesCombo" plugin configuration built correctly for default toolbar.');

    // Styles dropdown/button enabled: new setting should be present.
    $editor->settings['toolbar']['buttons'][0][] = 'Styles';
    $editor->settings['plugins']['stylescombo']['styles'] = '';
    $editor->save();
    $expected['stylesSet'] = array();
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor), '"StylesCombo" plugin configuration built correctly for customized toolbar.');

    // Configure the optional "styles" setting in odd ways that shouldn't affect
    // the end result.
    $editor->settings['plugins']['stylescombo']['styles'] = "   \n";
    $editor->save();
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor));
    $editor->settings['plugins']['stylescombo']['styles'] = "\r\n  \n  \r  \n ";
    $editor->save();
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor), '"StylesCombo" plugin configuration built correctly for customized toolbar.');

    // Now configure it properly, the end result should change.
    $editor->settings['plugins']['stylescombo']['styles'] = "h1.title|Title\np.mAgical.Callout|Callout";
    $editor->save();
    $expected['stylesSet'] = array(
      array('name' => 'Title', 'element' => 'h1', 'attributes' => array('class' => 'title')),
      array('name' => 'Callout', 'element' => 'p', 'attributes' => array('class' => 'mAgical Callout')),
    );
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor), '"StylesCombo" plugin configuration built correctly for customized toolbar.');

    // Same configuration, but now interspersed with nonsense. Should yield the
    // same result.
    $editor->settings['plugins']['stylescombo']['styles'] = "  h1 .title   |  Title \r \n\r  \np.mAgical  .Callout|Callout\r";
    $editor->save();
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor), '"StylesCombo" plugin configuration built correctly for customized toolbar.');

    // Invalid syntax should cause stylesSet to be set to FALSE.
    $editor->settings['plugins']['stylescombo']['styles'] = "h1|Title";
    $editor->save();
    $expected['stylesSet'] = FALSE;
    $this->assertIdentical($expected, $stylescombo_plugin->getConfig($editor), '"StylesCombo" plugin configuration built correctly for customized toolbar.');
  }

  protected function getDefaultInternalConfig() {
    return array(
      'customConfig' => '',
      'pasteFromWordPromptCleanup' => TRUE,
      'removeDialogTabs' => 'image:Link;image:advanced;link:advanced',
      'resize_dir' => 'vertical',
      'keystrokes' =>  array(array(0x110000 + 75, 'link'), array(0x110000 + 76, NULL)),
    );
  }

  protected function getDefaultStylesComboConfig() {
    return array();
  }

  protected function getDefaultToolbarConfig() {
    return array(
      0 => array('items' => array('Bold', 'Italic')),
      1 => array('items' => array('Link', 'Unlink')),
      2 => array('items' => array('BulletedList', 'NumberedList')),
      3 => array('items' => array('Blockquote', 'Image')),
      4 => array('items' => array('Source')),
      5 => '/'
    );
  }

  protected function getDefaultContentsCssConfig() {
    return array(
      file_create_url('core/modules/ckeditor/css/ckeditor-iframe.css'),
    );
  }

}
