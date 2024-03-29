<?php

/**
 * @file
 * Contains \Drupal\entity_test\Plugin\Core\Entity\EntityTestLabelCallback.
 */

namespace Drupal\entity_test\Plugin\Core\Entity;

use Drupal\Core\Annotation\Plugin;
use Drupal\Core\Annotation\Translation;

/**
 * Test entity class.
 *
 * @Plugin(
 *   id = "entity_test_label_callback",
 *   label = @Translation("Entity test label callback"),
 *   module = "entity_test",
 *   controller_class = "Drupal\entity_test\EntityTestStorageController",
 *   field_cache = FALSE,
 *   base_table = "entity_test",
 *   revision_table = "entity_test_revision",
 *   label_callback = "entity_test_label_callback",
 *   fieldable = TRUE,
 *   entity_keys = {
 *     "id" = "id"
 *   }
 * )
 */
class EntityTestLabelCallback extends EntityTest {

}
