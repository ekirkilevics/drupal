<?php

/**
 * @file
 * Contains \Drupal\entity_test\Plugin\Core\Entity\EntityTestLabel.
 */

namespace Drupal\entity_test\Plugin\Core\Entity;

use Drupal\Core\Annotation\Plugin;
use Drupal\Core\Annotation\Translation;

/**
 * Test entity class.
 *
 * @Plugin(
 *   id = "entity_test_label",
 *   label = @Translation("Entity Test label"),
 *   module = "entity_test",
 *   controller_class = "Drupal\entity_test\EntityTestStorageController",
 *   base_table = "entity_test",
 *   entity_keys = {
 *     "id" = "id",
 *     "label" = "name"
 *   }
 * )
 */
class EntityTestLabel extends EntityTest {

}
