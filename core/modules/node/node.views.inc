<?php

/**
 * @file
 * Provide views data and handlers for node.module.
 *
 * @ingroup views_module_handlers
 */

use Drupal\views\Analyzer;
use Drupal\views\ViewExecutable;

/**
 * Implements hook_views_data().
 */
function node_views_data() {

  // Define the base group of this table. Fields that don't have a group defined
  // will go into this field by default.
  $data['node']['table']['group'] = t('Content');

  // Advertise this table as a possible base table.
  $data['node']['table']['base'] = array(
    'field' => 'nid',
    'title' => t('Content'),
    'weight' => -10,
    'access query tag' => 'node_access',
    'defaults' => array(
      'field' => 'title',
    ),
  );
  $data['node']['table']['entity type'] = 'node';
  $data['node']['table']['wizard_id'] = 'node';

  $data['node']['nid'] = array(
    'title' => t('Nid'),
    'help' => t('The node ID.'),
    'field' => array(
      'id' => 'node',
    ),
    'argument' => array(
      'id' => 'node_nid',
      'name field' => 'title',
      'numeric' => TRUE,
      'validate type' => 'nid',
    ),
    'filter' => array(
      'id' => 'numeric',
    ),
    'sort' => array(
      'id' => 'standard',
    ),
  );

  // This definition has more items in it than it needs to as an example.
  $data['node']['title'] = array(
    'title' => t('Title'),
    'help' => t('The content title.'),
    'field' => array(
      // This is the real field which could be left out since it is the same.
      'field' => 'title',
      // This is the UI group which could be left out since it is the same.
      'group' => t('Content'),
      'id' => 'node',
      'link_to_node default' => TRUE,
     ),
    'sort' => array(
      'id' => 'standard',
    ),
    'filter' => array(
      'id' => 'string',
    ),
    'argument' => array(
      'id' => 'string',
    ),
  );

  $data['node']['created'] = array(
    'title' => t('Post date'),
    'help' => t('The date the content was posted.'),
    'field' => array(
      'id' => 'date',
    ),
    'sort' => array(
      'id' => 'date'
    ),
    'filter' => array(
      'id' => 'date',
    ),
  );

  $data['node']['changed'] = array(
    'title' => t('Updated date'),
    'help' => t('The date the content was last updated.'),
    'field' => array(
      'id' => 'date',
    ),
    'sort' => array(
      'id' => 'date'
    ),
    'filter' => array(
      'id' => 'date',
    ),
  );

  $data['node']['type'] = array(
    'title' => t('Type'),
    'help' => t('The content type (for example, "blog entry", "forum post", "story", etc).'),
    'field' => array(
      'id' => 'node_type',
    ),
    'sort' => array(
      'id' => 'standard',
    ),
    'filter' => array(
      'id' => 'bundle',
    ),
    'argument' => array(
      'id' => 'node_type',
    ),
  );

  $data['node']['status'] = array(
    'title' => t('Published status'),
    'help' => t('Whether or not the content is published.'),
    'field' => array(
      'id' => 'boolean',
      'output formats' => array(
        'published-notpublished' => array(t('Published'), t('Not published')),
      ),
    ),
    'filter' => array(
      'id' => 'boolean',
      'label' => t('Published status'),
      'type' => 'yes-no',
      // Use status = 1 instead of status <> 0 in WHERE statement.
      'use_equal' => TRUE,
    ),
    'sort' => array(
      'id' => 'standard',
    ),
  );

  $data['node']['status_extra'] = array(
    'title' => t('Published status or admin user'),
    'help' => t('Filters out unpublished content if the current user cannot view it.'),
    'filter' => array(
      'field' => 'status',
      'id' => 'node_status',
      'label' => t('Published status or admin user'),
    ),
  );

  $data['node']['promote'] = array(
    'title' => t('Promoted to front page status'),
    'help' => t('Whether or not the content is promoted to the front page.'),
    'field' => array(
      'id' => 'boolean',
      'output formats' => array(
        'promoted-notpromoted' => array(t('Promoted'), t('Not promoted')),
      ),
    ),
    'filter' => array(
      'id' => 'boolean',
      'label' => t('Promoted to front page status'),
      'type' => 'yes-no',
    ),
    'sort' => array(
      'id' => 'standard',
    ),
  );

  $data['node']['sticky'] = array(
    'title' => t('Sticky status'),
    'help' => t('Whether or not the content is sticky.'),
    'field' => array(
      'id' => 'boolean',
      'output formats' => array(
        'sticky' => array(t('Sticky'), t('Not sticky')),
      ),
    ),
    'filter' => array(
      'id' => 'boolean',
      'label' => t('Sticky status'),
      'type' => 'yes-no',
    ),
    'sort' => array(
      'id' => 'standard',
      'help' => t('Whether or not the content is sticky. To list sticky content first, set this to descending.'),
    ),
  );

  if (module_exists('language')) {
    $data['node']['langcode'] = array(
      'title' => t('Language'),
      'help' => t('The language the content is in.'),
      'field' => array(
        'id' => 'node_language',
      ),
      'filter' => array(
        'id' => 'language',
      ),
      'argument' => array(
        'id' => 'language',
      ),
      'sort' => array(
        'id' => 'standard',
      ),
    );
  }

  if (drupal_container()->get('module_handler')->moduleExists('translation_entity')) {
    $data['node']['translation_link'] = array(
      'title' => t('Translation link'),
      'help' => t('Provide a link to the translations overview for nodes.'),
      'field' => array(
        'id' => 'translation_entity_link',
      ),
    );
  }

  // Define some fields based upon views_handler_field_entity in the entity
  // table so they can be re-used with other query backends.
  // @see views_handler_field_entity

  // Define the base group of this table. Fields that don't have a group defined
  // will go into this field by default.
  $data['views_entity_node']['table']['group'] = t('Content');

  $data['views_entity_node']['view_node'] = array(
    'field' => array(
      'title' => t('Link to content'),
      'help' => t('Provide a simple link to the content.'),
      'id' => 'node_link',
    ),
  );

  $data['views_entity_node']['edit_node'] = array(
    'field' => array(
      'title' => t('Link to edit content'),
      'help' => t('Provide a simple link to edit the content.'),
      'id' => 'node_link_edit',
    ),
  );

  $data['views_entity_node']['delete_node'] = array(
    'field' => array(
      'title' => t('Link to delete content'),
      'help' => t('Provide a simple link to delete the content.'),
      'id' => 'node_link_delete',
    ),
  );

  $data['node']['path'] = array(
    'field' => array(
      'title' => t('Path'),
      'help' => t('The aliased path to this content.'),
      'id' => 'node_path',
    ),
  );

  // Bogus fields for aliasing purposes.

  $data['node']['created_fulldate'] = array(
    'title' => t('Created date'),
    'help' => t('Date in the form of CCYYMMDD.'),
    'argument' => array(
      'field' => 'created',
      'id' => 'node_created_fulldate',
    ),
  );

  $data['node']['created_year_month'] = array(
    'title' => t('Created year + month'),
    'help' => t('Date in the form of YYYYMM.'),
    'argument' => array(
      'field' => 'created',
      'id' => 'node_created_year_month',
    ),
  );

  $data['node']['created_year'] = array(
    'title' => t('Created year'),
    'help' => t('Date in the form of YYYY.'),
    'argument' => array(
      'field' => 'created',
      'id' => 'node_created_year',
    ),
  );

  $data['node']['created_month'] = array(
    'title' => t('Created month'),
    'help' => t('Date in the form of MM (01 - 12).'),
    'argument' => array(
      'field' => 'created',
      'id' => 'node_created_month',
    ),
  );

  $data['node']['created_day'] = array(
    'title' => t('Created day'),
    'help' => t('Date in the form of DD (01 - 31).'),
    'argument' => array(
      'field' => 'created',
      'id' => 'node_created_day',
    ),
  );

  $data['node']['created_week'] = array(
    'title' => t('Created week'),
    'help' => t('Date in the form of WW (01 - 53).'),
    'argument' => array(
      'field' => 'created',
      'id' => 'node_created_week',
    ),
  );

  $data['node']['changed_fulldate'] = array(
    'title' => t('Updated date'),
    'help' => t('Date in the form of CCYYMMDD.'),
    'argument' => array(
      'field' => 'changed',
      'id' => 'node_created_fulldate',
    ),
  );

  $data['node']['changed_year_month'] = array(
    'title' => t('Updated year + month'),
    'help' => t('Date in the form of YYYYMM.'),
    'argument' => array(
      'field' => 'changed',
      'id' => 'node_created_year_month',
    ),
  );

  $data['node']['changed_year'] = array(
    'title' => t('Updated year'),
    'help' => t('Date in the form of YYYY.'),
    'argument' => array(
      'field' => 'changed',
      'id' => 'node_created_year',
    ),
  );

  $data['node']['changed_month'] = array(
    'title' => t('Updated month'),
    'help' => t('Date in the form of MM (01 - 12).'),
    'argument' => array(
      'field' => 'changed',
      'id' => 'node_created_month',
    ),
  );

  $data['node']['changed_day'] = array(
    'title' => t('Updated day'),
    'help' => t('Date in the form of DD (01 - 31).'),
    'argument' => array(
      'field' => 'changed',
      'id' => 'node_created_day',
    ),
  );

  $data['node']['changed_week'] = array(
    'title' => t('Updated week'),
    'help' => t('Date in the form of WW (01 - 53).'),
    'argument' => array(
      'field' => 'changed',
      'id' => 'node_created_week',
    ),
  );

  $data['node']['uid'] = array(
    'title' => t('Author uid'),
    'help' => t('The user authoring the content. If you need more fields than the uid add the content: author relationship'),
    'relationship' => array(
      'title' => t('Content author'),
      'help' => t('Relate content to the user who created it.'),
      'id' => 'standard',
      'base' => 'users',
      'field' => 'uid',
      'label' => t('author'),
    ),
    'filter' => array(
      'id' => 'user_name',
    ),
    'argument' => array(
      'id' => 'numeric',
    ),
    'field' => array(
      'id' => 'user',
    ),
  );

  $data['node']['uid_revision'] = array(
    'title' => t('User has a revision'),
    'help' => t('All nodes where a certain user has a revision'),
    'real field' => 'nid',
    'filter' => array(
      'id' => 'node_uid_revision',
    ),
    'argument' => array(
      'id' => 'node_uid_revision',
    ),
  );

  $data['node_revision']['table']['entity type'] = 'node';
  // Define the base group of this table. Fields that don't have a group defined
  // will go into this field by default.
  $data['node_revision']['table']['group']  = t('Content revision');
  $data['node_revision']['table']['wizard_id'] = 'node_revision';


  // Advertise this table as a possible base table.
  $data['node_revision']['table']['base'] = array(
    'field' => 'vid',
    'title' => t('Content revision'),
    'help' => t('Content revision is a history of changes to content.'),
    'defaults' => array(
      'field' => 'title',
    ),
  );

  // For other base tables, explain how we join.
  $data['node_revision']['table']['join'] = array(
    'node' => array(
      'left_field' => 'vid',
      'field' => 'vid',
    ),
  );

  $data['node_revision']['uid'] = array(
    'title' => t('User'),
    'help' => t('Relate a content revision to the user who created the revision.'),
    'relationship' => array(
      'id' => 'standard',
      'base' => 'users',
      'base field' => 'uid',
      'label' => t('revision user'),
    ),
  );

  $data['node_revision']['nid'] = array(
    'title' => t('Nid'),
    'help' => t('The revision NID of the content revision.'),
    'field' => array(
      'id' => 'standard',
    ),
    'argument' => array(
      'id' => 'node_nid',
      'numeric' => TRUE,
    ),
    'filter' => array(
      'id' => 'numeric',
    ),
    'sort' => array(
      'id' => 'standard',
    ),
    'relationship' => array(
      'id' => 'standard',
      'base' => 'node',
      'base field' => 'nid',
      'title' => t('Content'),
      'label' => t('Get the actual content from a content revision.'),
    ),
  );

  $data['node_revision']['vid'] = array(
    'title' => t('Vid'),
    'help' => t('The revision ID of the content revision.'),
    'field' => array(
      'id' => 'standard',
    ),
    'argument' => array(
      'id' => 'node_vid',
      'numeric' => TRUE,
    ),
    'filter' => array(
      'id' => 'numeric',
    ),
    'sort' => array(
      'id' => 'standard',
    ),
    'relationship' => array(
      'id' => 'standard',
      'base' => 'node',
      'base field' => 'vid',
      'title' => t('Content'),
      'label' => t('Get the actual content from a content revision.'),
    ),
  );

  $data['node_revision']['status'] = array(
    'title' => t('Published'),
    'help' => t('Whether or not the content is published.'),
    'field' => array(
      'id' => 'boolean',
      'output formats' => array(
        'published-notpublished' => array(t('Published'), t('Not published')),
      ),
    ),
    'filter' => array(
      'id' => 'boolean',
      'label' => t('Published'),
      'type' => 'yes-no',
      // Use status = 1 instead of status <> 0 in WHERE statement.
      'use_equal' => TRUE,
    ),
    'sort' => array(
      'id' => 'standard',
    ),
  );

  $data['node_revision']['title'] = array(
    'title' => t('Title'),
    'help' => t('The content title.'),
    'field' => array(
      'field' => 'title',
      'id' => 'node_revision',
     ),
    'sort' => array(
      'id' => 'standard',
    ),
    'filter' => array(
      'id' => 'string',
    ),
    'argument' => array(
      'id' => 'string',
    ),
  );

  $data['node_revision']['log'] = array(
    'title' => t('Log message'),
    'help' => t('The log message entered when the revision was created.'),
    'field' => array(
      'id' => 'xss',
     ),
    'filter' => array(
      'id' => 'string',
    ),
  );

  $data['node_revision']['timestamp'] = array(
    'title' => t('Updated date'),
    'help' => t('The date the node was last updated.'),
    'field' => array(
      'id' => 'date',
    ),
    'sort' => array(
      'id' => 'date'
    ),
    'filter' => array(
      'id' => 'date',
    ),
  );

  $data['node_revision']['link_to_revision'] = array(
    'field' => array(
      'title' => t('Link to revision'),
      'help' => t('Provide a simple link to the revision.'),
      'id' => 'node_revision_link',
      'click sortable' => FALSE,
    ),
  );

  $data['node_revision']['revert_revision'] = array(
    'field' => array(
      'title' => t('Link to revert revision'),
      'help' => t('Provide a simple link to revert to the revision.'),
      'id' => 'node_revision_link_revert',
      'click sortable' => FALSE,
    ),
  );

  $data['node_revision']['delete_revision'] = array(
    'field' => array(
      'title' => t('Link to delete revision'),
      'help' => t('Provide a simple link to delete the content revision.'),
      'id' => 'node_revision_link_delete',
      'click sortable' => FALSE,
    ),
  );

  // Define the base group of this table. Fields that don't have a group defined
  // will go into this field by default.
  $data['node_access']['table']['group']  = t('Content access');

  // For other base tables, explain how we join.
  $data['node_access']['table']['join'] = array(
    'node' => array(
      'left_field' => 'nid',
      'field' => 'nid',
    ),
  );
  $data['node_access']['nid'] = array(
    'title' => t('Access'),
    'help' => t('Filter by access.'),
    'filter' => array(
      'id' => 'node_access',
      'help' => t('Filter for content by view access. <strong>Not necessary if you are using node as your base table.</strong>'),
    ),
  );

  return $data;
}

/**
 * Implements hook_preprocess_node().
 */
function node_row_node_view_preprocess_node(&$vars) {
  $node = $vars['node'];
  $options = $vars['view']->style_plugin->row_plugin->options;

  // Prevent the comment form from showing up if this is not a page display.
  if ($vars['view_mode'] == 'full' && !$vars['view']->display_handler->hasPath()) {
    $node->comment = FALSE;
  }

  if (!$options['links']) {
    unset($vars['content']['links']);
  }

  if (!empty($options['comments']) && user_access('access comments') && $node->comment) {
    $vars['content']['comments'] = comment_node_page_additions($node);
  }
}

/**
 * Implements hook_views_query_substitutions().
 */
function node_views_query_substitutions() {
  return array(
    '***ADMINISTER_NODES***' => intval(user_access('administer nodes')),
    '***VIEW_OWN_UNPUBLISHED_NODES***' => intval(user_access('view own unpublished content')),
    '***BYPASS_NODE_ACCESS***' =>  intval(user_access('bypass node access')),
  );
}

/**
 * Implements hook_views_analyze().
 */
function node_views_analyze(ViewExecutable $view) {
  $ret = array();
  // Check for something other than the default display:
  if ($view->storage->get('base_table') == 'node') {
    foreach ($view->displayHandlers as $id => $display) {
      if (!$display->isDefaulted('access') || !$display->isDefaulted('filters')) {
        // check for no access control
        $access = $display->getOption('access');
        if (empty($access['type']) || $access['type'] == 'none') {
          $result = db_select('role_permission', 'p')
            ->fields('p', array('rid', 'permission'))
            ->condition('p.rid', array(DRUPAL_ANONYMOUS_RID, DRUPAL_AUTHENTICATED_RID), 'IN')
            ->condition('p.permission', 'access content')
            ->execute();

          foreach ($result as $role) {
            $role->safe = TRUE;
            $roles[$role->rid] = $role;
          }
          if (!($roles[DRUPAL_ANONYMOUS_RID]->safe && $roles[DRUPAL_AUTHENTICATED_RID]->safe)) {
            $ret[] = Analyzer::formatMessage(t('Some roles lack permission to access content, but display %display has no access control.', array('%display' => $display->display['display_title'])), 'warning');
          }
          $filters = $display->getOption('filters');
          foreach ($filters as $filter) {
            if ($filter['table'] == 'node' && ($filter['field'] == 'status' || $filter['field'] == 'status_extra')) {
              continue 2;
            }
          }
          $ret[] = Analyzer::formatMessage(t('Display %display has no access control but does not contain a filter for published nodes.', array('%display' => $display->display['display_title'])), 'warning');
        }
      }
    }
  }
  foreach ($view->displayHandlers as $display) {
    if ($display->getPluginId() == 'page') {
      if ($display->getOption('path') == 'node/%') {
        $ret[] = Analyzer::formatMessage(t('Display %display has set node/% as path. This will not produce what you want. If you want to have multiple versions of the node view, use panels.', array('%display' => $display->display['display_title'])), 'warning');
      }
    }
  }

  return $ret;
}

/**
 * Implements hook_views_wizard().
 */
function node_views_wizard() {
  // @todo: figure this piece out.
  if (module_exists('statistics')) {
    $plugins['node']['available_sorts']['node_counter-totalcount:DESC'] = t('Number of hits');
  }

}
