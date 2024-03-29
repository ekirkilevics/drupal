<?php

/**
 * @file
 * Definition of Drupal\rest\test\RESTTestBase.
 */

namespace Drupal\rest\Tests;

use Drupal\simpletest\WebTestBase;

/**
 * Test helper class that provides a REST client method to send HTTP requests.
 */
abstract class RESTTestBase extends WebTestBase {

  /**
   * Helper function to issue a HTTP request with simpletest's cURL.
   *
   * @param string $url
   *   The relative URL, e.g. "entity/node/1"
   * @param string $method
   *   HTTP method, one of GET, POST, PUT or DELETE.
   * @param array $body
   *   Either the body for POST and PUT or additional URL parameters for GET.
   * @param string $format
   *   The MIME type of the transmitted content.
   */
  protected function httpRequest($url, $method, $body = NULL, $format = 'application/ld+json') {
    if (!in_array($method, array('GET', 'HEAD', 'OPTIONS', 'TRACE'))) {
      // GET the CSRF token first for writing requests.
      $token = $this->drupalGet('rest/session/token');
    }
    switch ($method) {
      case 'GET':
        // Set query if there are additional GET parameters.
        $options = isset($body) ? array('absolute' => TRUE, 'query' => $body) : array('absolute' => TRUE);
        $curl_options = array(
          CURLOPT_HTTPGET => TRUE,
          CURLOPT_URL => url($url, $options),
          CURLOPT_NOBODY => FALSE,
          CURLOPT_HTTPHEADER => array('Accept: ' . $format),
        );
        break;

      case 'POST':
        $curl_options = array(
          CURLOPT_HTTPGET => FALSE,
          CURLOPT_POST => TRUE,
          CURLOPT_POSTFIELDS => $body,
          CURLOPT_URL => url($url, array('absolute' => TRUE)),
          CURLOPT_NOBODY => FALSE,
          CURLOPT_HTTPHEADER => array(
            'Content-Type: ' . $format,
            'X-CSRF-Token: ' . $token,
          ),
        );
        break;

      case 'PUT':
        $curl_options = array(
          CURLOPT_HTTPGET => FALSE,
          CURLOPT_CUSTOMREQUEST => 'PUT',
          CURLOPT_POSTFIELDS => $body,
          CURLOPT_URL => url($url, array('absolute' => TRUE)),
          CURLOPT_NOBODY => FALSE,
          CURLOPT_HTTPHEADER => array(
            'Content-Type: ' . $format,
            'X-CSRF-Token: ' . $token,
          ),
        );
        break;

      case 'PATCH':
        $curl_options = array(
          CURLOPT_HTTPGET => FALSE,
          CURLOPT_CUSTOMREQUEST => 'PATCH',
          CURLOPT_POSTFIELDS => $body,
          CURLOPT_URL => url($url, array('absolute' => TRUE)),
          CURLOPT_NOBODY => FALSE,
          CURLOPT_HTTPHEADER => array(
            'Content-Type: ' . $format,
            'X-CSRF-Token: ' . $token,
          ),
        );
        break;

      case 'DELETE':
        $curl_options = array(
          CURLOPT_HTTPGET => FALSE,
          CURLOPT_CUSTOMREQUEST => 'DELETE',
          CURLOPT_URL => url($url, array('absolute' => TRUE)),
          CURLOPT_NOBODY => FALSE,
          CURLOPT_HTTPHEADER => array('X-CSRF-Token: ' . $token),
        );
        break;
    }

    $response = $this->curlExec($curl_options);
    $headers = $this->drupalGetHeaders();
    $headers = implode("\n", $headers);

    $this->verbose($method . ' request to: ' . $url .
      '<hr />Code: ' . curl_getinfo($this->curlHandle, CURLINFO_HTTP_CODE) .
      '<hr />Response headers: ' . $headers .
      '<hr />Response body: ' . $response);

    return $response;
  }

  /**
   * Creates entity objects based on their types.
   *
   * @param string $entity_type
   *   The type of the entity that should be created.
   *
   * @return \Drupal\Core\Entity\EntityInterface
   *   The new entity object.
   */
  protected function entityCreate($entity_type) {
    return entity_create($entity_type, $this->entityValues($entity_type));
  }

  /**
   * Provides an array of suitable property values for an entity type.
   *
   * Required properties differ from entity type to entity type, so we keep a
   * minimum mapping here.
   *
   * @param string $entity_type
   *   The type of the entity that should be created.
   *
   * @return array
   *   An array of values keyed by property name.
   */
  protected function entityValues($entity_type) {
    switch ($entity_type) {
      case 'entity_test':
        return array(
          'name' => $this->randomName(),
          'user_id' => 1,
          'field_test_text' => array(0 => array('value' => $this->randomString())),
        );
      case 'node':
        return array('title' => $this->randomString());
      case 'user':
        return array('name' => $this->randomName());
      default:
        return array();
    }
  }

  /**
   * Enables the web service interface for a specific entity type.
   *
   * @param string|FALSE $resource_type
   *   The resource type that should get web API enabled or FALSE to disable all
   *   resource types.
   * @param string $method
   *   The HTTP method to enable, e.g. GET, POST etc.
   * @param string $format
   *   (Optional) The serialization format, e.g. jsonld.
   */
  protected function enableService($resource_type, $method = 'GET', $format = NULL) {
    // Enable web API for this entity type.
    $config = config('rest.settings');
    $settings = array();
    if ($resource_type) {
      if ($format) {
        $settings[$resource_type][$method][$format] = 'TRUE';
      }
      else {
        $settings[$resource_type][$method] = array();
      }
    }
    $config->set('resources', $settings);
    $config->save();

    // Rebuild routing cache, so that the web API paths are available.
    drupal_container()->get('router.builder')->rebuild();
    // Reset the Simpletest permission cache, so that the new resource
    // permissions get picked up.
    drupal_static_reset('checkPermissions');
  }

  /**
   * Check if a HTTP response header exists and has the expected value.
   *
   * @param string $header
   *   The header key, example: Content-Type
   * @param string $value
   *   The header value.
   * @param string $message
   *   (optional) A message to display with the assertion.
   * @param string $group
   *   (optional) The group this message is in, which is displayed in a column
   *   in test output. Use 'Debug' to indicate this is debugging output. Do not
   *   translate this string. Defaults to 'Other'; most tests do not override
   *   this default.
   *
   * @return bool
   *   TRUE if the assertion succeeded, FALSE otherwise.
   */
  protected function assertHeader($header, $value, $message = '', $group = 'Browser') {
    $header_value = $this->drupalGetHeader($header);
    return $this->assertTrue($header_value == $value, $message ? $message : 'HTTP response header ' . $header . ' with value ' . $value . ' found.', $group);
  }

  /**
   * Overrides WebTestBase::drupalLogin().
   */
  protected function drupalLogin($user) {
    if (isset($this->curlHandle)) {
      // cURL quirk: when setting CURLOPT_CUSTOMREQUEST to anything other than
      // POST in httpRequest() it has to be restored to POST here. Otherwise the
      // POST request to login a user will not work.
      curl_setopt($this->curlHandle, CURLOPT_CUSTOMREQUEST, 'POST');
    }
    parent::drupalLogin($user);
  }
}
