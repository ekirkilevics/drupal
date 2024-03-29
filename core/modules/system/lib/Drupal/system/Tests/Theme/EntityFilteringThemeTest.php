<?php

/**
 * @file
 * Contains Drupal\system\Tests\Theme\EntityFilteringThemeTest.
 */

namespace Drupal\system\Tests\Theme;

use Drupal\simpletest\WebTestBase;

/**
 * Tests filtering for XSS in rendered entity templates in all themes.
 */
class EntityFilteringThemeTest extends WebTestBase {

  /**
   * Use the standard profile.
   *
   * We test entity theming with the default node, user, comment, and taxonomy
   * configurations at several paths in the standard profile.
   *
   * @var string
   */
  protected $profile = 'standard';

  /**
   * A list of all available themes.
   *
   * @var array
   */
  protected $themes;

  /**
   * A test user.
   *
   * @var Drupal\user\User
   */
  protected $user;


  /**
   * A test node.
   *
   * @var Drupal\node\Node
   */
  protected $node;


  /**
   * A test taxonomy term.
   *
   * @var Drupal\taxonomy\Term
   */
  protected $term;


  /**
   * A test comment.
   *
   * @var Drupal\comment\Comment
   */
  protected $comment;

  /**
   * A string containing markup and JS.
   *
   * @string
   */
  protected $xss_label = "string with <em>HTML</em> and <script>alert('JS');</script>";

  public static function getInfo() {
    return array(
      'name' => 'Entity filtering theme test',
      'description' => 'Tests themed output for each entity type in all available themes to ensure entity labels are filtered for XSS.',
      'group' => 'Theme',
    );
  }

  function setUp() {
    parent::setUp();

    // Enable all available themes for testing.
    $this->themes = array_keys(list_themes());
    theme_enable($this->themes);

    // Create a test user.
    $this->user = $this->drupalCreateUser(array('access content', 'access user profiles'));
    $this->user->name = $this->xss_label;
    $this->user->save();
    $this->drupalLogin($this->user);

    // Create a test term.
    $this->term = entity_create('taxonomy_term', array(
      'name' => $this->xss_label,
      'vid' => 1,
    ));
    taxonomy_term_save($this->term);

    // Create a test node tagged with the test term.
    $this->node = $this->drupalCreateNode(array(
      'title' => $this->xss_label,
      'type' => 'article',
      'promote' => NODE_PROMOTED,
      'field_tags' => array(LANGUAGE_NOT_SPECIFIED => array(array('tid' => $this->term->tid))),
    ));

    // Create a test comment on the test node.
    $this->comment = entity_create('comment', array(
      'nid' => $this->node->nid,
      'node_type' => $this->node->type,
      'status' => COMMENT_PUBLISHED,
      'subject' => $this->xss_label,
      'comment_body' => array(LANGUAGE_NOT_SPECIFIED => array($this->randomName())),
    ));
    comment_save($this->comment);
  }

  /**
   * Checks each themed entity for XSS filtering in available themes.
   */
  function testThemedEntity() {
    // Check paths where various view modes of the entities are rendered.
    $paths = array(
      'user',
      'node',
      'node/' . $this->node->nid,
      'taxonomy/term/' . $this->term->tid,
    );

    // Check each path in all available themes.
    foreach ($this->themes as $theme) {
      config('system.theme')
        ->set('default', $theme)
        ->save();
      foreach ($paths as $path) {
        $this->drupalGet($path);
        $this->assertResponse(200);
        $this->assertNoRaw($this->xss_label);
      }
    }
  }

}
