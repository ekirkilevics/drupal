<?php

/**
 * @file
 * Contains \Drupal\condition_test\FormController.
 */

namespace Drupal\condition_test;

use Drupal\Core\Form\FormInterface;
use Drupal\Core\Condition\ConditionManager;

/**
 * Routing controller class for condition_test testing of condition forms.
 */
class FormController implements FormInterface {

  /**
   * The condition plugin we will be working with.
   *
   * @var \Drupal\Core\Condition\ConditionInterface
   */
  protected $condition;

  /**
   * Implements \Drupal\Core\Form\FormInterface::getFormID().
   */
  public function getFormID() {
    return 'condition_node_type_test_form';
  }

  /**
   * Provides a simple method the router can fire in order to invoke this form.
   */
  public function getForm() {
    $manager = new ConditionManager(drupal_container()->getParameter('container.namespaces'));
    $this->condition = $manager->createInstance('node_type');
    return drupal_get_form($this);
  }

  /**
   * Implements \Drupal\Core\Form\FormInterface::buildForm().
   */
  public function buildForm(array $form, array &$form_state) {
    $form = $this->condition->buildForm($form, $form_state);
    $form['actions']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Submit'),
    );
    return $form;
  }

  /**
   * Implements \Drupal\Core\Form\FormInterface::validateForm().
   */
  public function validateForm(array &$form, array &$form_state) {
    $this->condition->validateForm($form, $form_state);
  }

  /**
   * Implements \Drupal\Core\Form\FormInterface::submitForm().
   */
  public function submitForm(array &$form, array &$form_state) {
    $this->condition->submitForm($form, $form_state);
    $config = $this->condition->getConfig();
    $bundles = implode(' and ', $config['bundles']);
    drupal_set_message(t('The bundles are @bundles', array('@bundles' => $bundles)));
    $article = node_load(1);
    $this->condition->setContextValue('node', $article);
    if ($this->condition->execute()) {
      drupal_set_message(t('Executed successfully.'));
    }
  }
}
