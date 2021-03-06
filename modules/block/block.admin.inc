<?php
// $Id: block.admin.inc,v 1.80 2010-05-13 07:53:02 dries Exp $

/**
 * @file
 * Admin page callbacks for the block module.
 */

/**
 * Menu callback for admin/structure/block/demo.
 */
function block_admin_demo($theme = NULL) {
  drupal_add_css(drupal_get_path('module', 'block') . '/block.css', array('preprocess' => FALSE));
  return '';
}

/**
 * Menu callback for admin/structure/block.
 *
 * @param $theme
 *   The theme to display the administration page for. If not provided, defaults
 *   to the currently used theme.
 */
function block_admin_display($theme = NULL) {
  global $theme_key;

  drupal_theme_initialize();

  if (!isset($theme)) {
    // If theme is not specifically set, rehash for the current theme.
    $theme = $theme_key;
  }

  // Fetch and sort blocks.
  $blocks = _block_rehash($theme);
  $compare_theme = &drupal_static('_block_compare:theme');
  $compare_theme = $theme;
  usort($blocks, '_block_compare');

  return drupal_get_form('block_admin_display_form', $blocks, $theme);
}

/**
 * Generate main blocks administration form.
 */
function block_admin_display_form($form, &$form_state, $blocks, $theme) {

  drupal_add_css(drupal_get_path('module', 'block') . '/block.css', array('preprocess' => FALSE));

  $block_regions = system_region_list($theme, REGIONS_VISIBLE) + array(BLOCK_REGION_NONE => '<' . t('none') . '>');

  // Weights range from -delta to +delta, so delta should be at least half
  // of the amount of blocks present. This makes sure all blocks in the same
  // region get an unique weight.
  $weight_delta = round(count($blocks) / 2);

  // Build the form tree.
  $form['edited_theme'] = array('#type' => 'value', '#value' => $theme);
  $form['#action'] = arg(4) ? url('admin/structure/block/list/' . $theme) : url('admin/structure/block');
  $form['#tree'] = TRUE;

  foreach ($blocks as $i => $block) {
    $key = $block['module'] . '_' . $block['delta'];
    $form[$key]['module'] = array(
      '#type' => 'value',
      '#value' => $block['module'],
    );
    $form[$key]['delta'] = array(
      '#type' => 'value',
      '#value' => $block['delta'],
    );
    $form[$key]['info'] = array(
      '#markup' => check_plain($block['info']),
    );
    $form[$key]['theme'] = array(
      '#type' => 'hidden',
      '#value' => $theme,
    );
    $form[$key]['weight'] = array(
      '#type' => 'weight',
      '#default_value' => $block['weight'],
      '#delta' => $weight_delta,
    );
    $form[$key]['region'] = array(
      '#type' => 'select',
      '#default_value' => $block['region'],
      '#options' => $block_regions,
    );
    $form[$key]['configure'] = array(
      '#type' => 'link',
      '#title' => t('configure'),
      '#href' => 'admin/structure/block/manage/' . $block['module'] . '/' . $block['delta'] . '/configure',
    );
    if ($block['module'] == 'block') {
      $form[$key]['delete'] = array(
        '#type' => 'link',
        '#title' => t('delete'),
        '#href' => 'admin/structure/block/manage/' . $block['module'] . '/' . $block['delta'] . '/delete',
     );
    }
  }
  // Do not allow disabling the main system content block.
  unset($form['system_main']['region']['#options'][BLOCK_REGION_NONE]);

  $form['actions'] = array(
    '#tree' => FALSE,
    '#type' => 'actions',
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save blocks'),
  );

  return $form;
}

/**
 * Process main blocks administration form submissions.
 */
function block_admin_display_form_submit($form, &$form_state) {
  $txn = db_transaction();

  foreach ($form_state['values'] as $block) {
    $block['status'] = (int) ($block['region'] != BLOCK_REGION_NONE);
    $block['region'] = $block['status'] ? $block['region'] : '';
    db_update('block')
      ->fields(array(
        'status' => $block['status'],
        'weight' => $block['weight'],
        'region' => $block['region'],
      ))
      ->condition('module', $block['module'])
      ->condition('delta', $block['delta'])
      ->condition('theme', $block['theme'])
      ->execute();
  }
  drupal_set_message(t('The block settings have been updated.'));
  cache_clear_all();
}

/**
 * Helper function for sorting blocks on admin/structure/block.
 *
 * Active blocks are sorted by region, then by weight.
 * Disabled blocks are sorted by name.
 */
function _block_compare($a, $b) {
  global $theme_key;

  // Theme should be set before calling this function, or the current theme
  // is being used.
  $theme = &drupal_static(__FUNCTION__ . ':theme');
  if (!isset($theme)) {
    $theme = $theme_key;
  }

  $regions = &drupal_static(__FUNCTION__ . ':regions');
  // We need the region list to correctly order by region.
  if (!isset($regions)) {
    $regions = array_flip(array_keys(system_region_list($theme)));
    $regions[BLOCK_REGION_NONE] = count($regions);
  }

  // Separate enabled from disabled.
  $status = $b['status'] - $a['status'];
  if ($status) {
    return $status;
  }
  // Sort by region (in the order defined by theme .info file).
  if ((!empty($a['region']) && !empty($b['region'])) && ($place = ($regions[$a['region']] - $regions[$b['region']]))) {
    return $place;
  }
  // Sort by weight, unless disabled.
  if ($a['region'] != BLOCK_REGION_NONE) {
    $weight = $a['weight'] - $b['weight'];
    if ($weight) {
      return $weight;
    }
  }
  // Sort by title.
  return strcmp($a['info'], $b['info']);
}

/**
 * Menu callback; displays the block configuration form.
 */
function block_admin_configure($form, &$form_state, $module, $delta) {
  $block = block_load($module, $delta);
  $form['module'] = array(
    '#type' => 'value',
    '#value' => $block->module,
  );
  $form['delta'] = array(
    '#type' => 'value',
    '#value' => $block->delta,
  );

  // Get the block subject for the page title.
  $info = module_invoke($block->module, 'block_info');
  if (isset($info[$block->delta])) {
    drupal_set_title(t("'%name' block", array('%name' => $info[$block->delta]['info'])), PASS_THROUGH);
  }

  $form['settings']['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Block title'),
    '#maxlength' => 64,
    '#description' => $block->module == 'block' ? t('The title of the block as shown to the user.') : t('Override the default title for the block. Use <em>!placeholder</em> to display no title, or leave blank to use the default block title.', array('!placeholder' => '&lt;none&gt;')),
    '#default_value' => isset($block->title) ? $block->title : '',
    '#weight' => -18,
  );

  // Module-specific block configuration.
  if ($settings = module_invoke($block->module, 'block_configure', $block->delta)) {
    foreach ($settings as $k => $v) {
      $form['settings'][$k] = $v;
    }
  }

  // Region settings.
  $form['regions'] = array(
    '#type' => 'fieldset',
    '#title' => t('Region settings'),
    '#collapsible' => FALSE,
    '#description' => t('Specify in which themes and regions this block is displayed.'),
    '#tree' => TRUE,
  );

  $theme_default = variable_get('theme_default', 'garland');
  foreach (list_themes() as $key => $theme) {
    // Only display enabled themes
    if ($theme->status) {
      $region = db_query("SELECT region FROM {block} WHERE module = :module AND delta = :delta AND theme = :theme", array(
        ':module' => $block->module,
        ':delta' => $block->delta,
        ':theme' => $key,
      ))->fetchField();

      $form['regions'][$key] = array(
        '#type' => 'select',
        '#title' => $theme->info['name'],
        '#default_value' => (!empty($region) ? $region : BLOCK_REGION_NONE),
        '#options' => array(BLOCK_REGION_NONE => t('<none>')) + system_region_list($key, REGIONS_VISIBLE),
        '#expandable' => ($key !== $theme_default),
        '#weight' => ($key == $theme_default ? 9 : 10),
      );
    }
  }

  // Visibility settings.
  $form['visibility_title'] = array(
    '#type' => 'item',
    '#title' => t('Visibility settings'),
  );
  $form['visibility'] = array(
    '#type' => 'vertical_tabs',
    '#attached' => array(
      'js' => array(drupal_get_path('module', 'block') . '/block.js'),
    ),
  );

  // Per-path visibility.
  $form['visibility']['path'] = array(
    '#type' => 'fieldset',
    '#title' => t('Pages'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#group' => 'visibility',
    '#weight' => 0,
  );

  $access = user_access('use PHP for settings');
  if (isset($block->visibility) && $block->visibility == 2 && !$access) {
    $form['visibility']['path']['visibility'] = array(
      '#type' => 'value',
      '#value' => 2,
    );
    $form['visibility']['path']['pages'] = array(
      '#type' => 'value',
      '#value' => isset($block->pages) ? $block->pages : '',
    );
  }
  else {
    $options = array(
      t('All pages except those listed'),
      t('Only the listed pages'),
    );
    $description = t("Specify pages by using their paths. Enter one path per line. The '*' character is a wildcard. Example paths are %blog for the blog page and %blog-wildcard for every personal blog. %front is the front page.", array('%blog' => 'blog', '%blog-wildcard' => 'blog/*', '%front' => '<front>'));

    if (module_exists('php') && $access) {
      $options[] = t('Pages on which this PHP code returns <code>TRUE</code> (experts only)');
      $title = t('Pages or PHP code');
      $description .= ' ' . t('If the PHP option is chosen, enter PHP code between %php. Note that executing incorrect PHP code can break your Drupal site.', array('%php' => '<?php ?>'));
    }
    else {
      $title = t('Pages');
    }
    $form['visibility']['path']['visibility'] = array(
      '#type' => 'radios',
      '#title' => t('Show block on specific pages'),
      '#options' => $options,
      '#default_value' => isset($block->visibility) ? $block->visibility : 0,
    );
    $form['visibility']['path']['pages'] = array(
      '#type' => 'textarea',
      '#title' => '<span class="element-invisible">' . $title . '</span>',
      '#default_value' => isset($block->pages) ? $block->pages : '',
      '#description' => $description,
    );
  }

  // Per-role visibility.
  $default_role_options = db_query("SELECT rid FROM {block_role} WHERE module = :module AND delta = :delta", array(
    ':module' => $block->module,
    ':delta' => $block->delta,
  ))->fetchCol();
  $role_options = array_map('check_plain', user_roles());
  $form['visibility']['role'] = array(
    '#type' => 'fieldset',
    '#title' => t('Roles'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#group' => 'visibility',
    '#weight' => 10,
  );
  $form['visibility']['role']['roles'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Show block for specific roles'),
    '#default_value' => $default_role_options,
    '#options' => $role_options,
    '#description' => t('Show this block only for the selected role(s). If you select no roles, the block will be visible to all users.'),
  );

  // Per-user visibility.
  $form['visibility']['user'] = array(
    '#type' => 'fieldset',
    '#title' => t('Users'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#group' => 'visibility',
    '#weight' => 20,
  );
  $form['visibility']['user']['custom'] = array(
    '#type' => 'radios',
    '#title' => t('Customizable per user'),
    '#options' => array(
      t('Not customizable'),
      t('Customizable, visible by default'),
      t('Customizable, hidden by default'),
    ),
    '#description' => t('Allow individual users to customize the visibility of this block in their account settings.'),
    '#default_value' => isset($block->custom) ? $block->custom : 0,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save block'),
  );

  return $form;
}

function block_admin_configure_validate($form, &$form_state) {
  if ($form_state['values']['module'] == 'block') {
    $custom_block_exists = (bool) db_query_range('SELECT 1 FROM {block_custom} WHERE bid <> :bid AND info = :info', 0, 1, array(
      ':bid' => $form_state['values']['delta'],
      ':info' => $form_state['values']['info'],
    ))->fetchField();
    if (empty($form_state['values']['info']) || $custom_block_exists) {
      form_set_error('info', t('Ensure that each block description is unique.'));
    }
  }
}

function block_admin_configure_submit($form, &$form_state) {
  if (!form_get_errors()) {
    $txn = db_transaction();

    db_update('block')
      ->fields(array(
        'visibility' => (int) $form_state['values']['visibility'],
        'pages' => trim($form_state['values']['pages']),
        'custom' => (int) $form_state['values']['custom'],
        'title' => $form_state['values']['title'],
      ))
      ->condition('module', $form_state['values']['module'])
      ->condition('delta', $form_state['values']['delta'])
      ->execute();

    db_delete('block_role')
      ->condition('module', $form_state['values']['module'])
      ->condition('delta', $form_state['values']['delta'])
      ->execute();
    $query = db_insert('block_role')->fields(array('rid', 'module', 'delta'));
    foreach (array_filter($form_state['values']['roles']) as $rid) {
      $query->values(array(
        'rid' => $rid,
        'module' => $form_state['values']['module'],
        'delta' => $form_state['values']['delta'],
      ));
    }
    $query->execute();

    // Store regions per theme for this block
    foreach ($form_state['values']['regions'] as $theme => $region) {
      db_merge('block')
        ->key(array('theme' => $theme, 'delta' => $form_state['values']['delta'], 'module' => $form_state['values']['module']))
        ->fields(array(
          'region' => ($region == BLOCK_REGION_NONE ? '' : $region),
          'pages' => trim($form_state['values']['pages']),
          'status' => (int) ($region != BLOCK_REGION_NONE),
        ))
        ->execute();
    }

    module_invoke($form_state['values']['module'], 'block_save', $form_state['values']['delta'], $form_state['values']);
    drupal_set_message(t('The block configuration has been saved.'));
    cache_clear_all();
    $form_state['redirect'] = 'admin/structure/block';
  }
}

/**
 * Menu callback: display the custom block addition form.
 */
function block_add_block_form($form, &$form_state) {
  return block_admin_configure($form, $form_state, 'block', NULL);
}

function block_add_block_form_validate($form, &$form_state) {
  $custom_block_exists = (bool) db_query_range('SELECT 1 FROM {block_custom} WHERE info = :info', 0, 1, array(':info' => $form_state['values']['info']))->fetchField();

  if (empty($form_state['values']['info']) || $custom_block_exists) {
    form_set_error('info', t('Ensure that each block description is unique.'));
  }
}

/**
 * Save the new custom block.
 */
function block_add_block_form_submit($form, &$form_state) {
  $delta = db_insert('block_custom')
    ->fields(array(
      'body' => $form_state['values']['body']['value'],
      'info' => $form_state['values']['info'],
      'format' => $form_state['values']['body']['format'],
    ))
    ->execute();
  // Store block delta to allow other modules to work with new block.
  $form_state['values']['delta'] = $delta;

  $query = db_insert('block')->fields(array('visibility', 'pages', 'custom', 'title', 'module', 'theme', 'status', 'weight', 'delta', 'cache'));
  foreach (list_themes() as $key => $theme) {
    if ($theme->status) {
      $query->values(array(
        'visibility' => (int) $form_state['values']['visibility'],
        'pages' => trim($form_state['values']['pages']),
        'custom' => (int) $form_state['values']['custom'],
        'title' => $form_state['values']['title'],
        'module' => $form_state['values']['module'],
        'theme' => $theme->name,
        'status' => 0,
        'weight' => 0,
        'delta' => $delta,
        'cache' => DRUPAL_NO_CACHE,
      ));
    }
  }
  $query->execute();

  $query = db_insert('block_role')->fields(array('rid', 'module', 'delta'));
  foreach (array_filter($form_state['values']['roles']) as $rid) {
    $query->values(array(
      'rid' => $rid,
      'module' => $form_state['values']['module'],
      'delta' => $delta,
    ));
  }
  $query->execute();

  // Store regions per theme for this block
  foreach ($form_state['values']['regions'] as $theme => $region) {
    db_merge('block')
      ->key(array('theme' => $theme, 'delta' => $delta, 'module' => $form_state['values']['module']))
      ->fields(array(
        'region' => ($region == BLOCK_REGION_NONE ? '' : $region),
        'pages' => trim($form_state['values']['pages']),
        'status' => (int) ($region != BLOCK_REGION_NONE),
      ))
      ->execute();
  }

  drupal_set_message(t('The block has been created.'));
  cache_clear_all();
  $form_state['redirect'] = 'admin/structure/block';
}

/**
 * Menu callback; confirm deletion of custom blocks.
 */
function block_custom_block_delete($form, &$form_state, $module, $delta) {
  $block = block_load($module, $delta);
  $custom_block = block_custom_block_get($block->delta);
  $form['info'] = array('#type' => 'hidden', '#value' => $custom_block['info'] ? $custom_block['info'] : $custom_block['title']);
  $form['bid'] = array('#type' => 'hidden', '#value' => $block->delta);

  return confirm_form($form, t('Are you sure you want to delete the block %name?', array('%name' => $custom_block['info'])), 'admin/structure/block', '', t('Delete'), t('Cancel'));
}

/**
 * Deletion of custom blocks.
 */
function block_custom_block_delete_submit($form, &$form_state) {
  db_delete('block_custom')
    ->condition('bid', $form_state['values']['bid'])
    ->execute();
  db_delete('block')
    ->condition('module', 'block')
    ->condition('delta', $form_state['values']['bid'])
    ->execute();
  db_delete('block_role')
    ->condition('module', 'block')
    ->condition('delta', $form_state['values']['bid'])
    ->execute();
  drupal_set_message(t('The block %name has been removed.', array('%name' => $form_state['values']['info'])));
  cache_clear_all();
  $form_state['redirect'] = 'admin/structure/block';
  return;
}

/**
 * Process variables for block-admin-display.tpl.php.
 *
 * The $variables array contains the following arguments:
 * - $form
 *
 * @see block-admin-display.tpl.php
 * @see theme_block_admin_display()
 */
function template_preprocess_block_admin_display_form(&$variables) {

  $block_regions = system_region_list($variables['form']['edited_theme']['#value'], REGIONS_VISIBLE);
  $variables['block_regions'] = $block_regions + array(BLOCK_REGION_NONE => t('Disabled'));

  foreach ($block_regions as $key => $value) {
    // Initialize an empty array for the region.
    $variables['block_listing'][$key] = array();
  }

  // Initialize disabled blocks array.
  $variables['block_listing'][BLOCK_REGION_NONE] = array();

  // Set up to track previous region in loop.
  $last_region = '';
  foreach (element_children($variables['form']) as $i) {
    $block = &$variables['form'][$i];

    // Only take form elements that are blocks.
    if (isset($block['info'])) {
      // Fetch region for current block.
      $region = $block['region']['#default_value'];

      // Set special classes needed for table drag and drop.
      $variables['form'][$i]['region']['#attributes']['class'] = array('block-region-select', 'block-region-' . $region);
      $variables['form'][$i]['weight']['#attributes']['class'] = array('block-weight', 'block-weight-' . $region);

      $variables['block_listing'][$region][$i] = new stdClass();
      $variables['block_listing'][$region][$i]->row_class = !empty($block['#attributes']['class']) ? implode(' ', $block['#attributes']['class']) : '';
      $variables['block_listing'][$region][$i]->block_modified = !empty($block['#attributes']['class']) && in_array('block-modified', $block['#attributes']['class']);
      $variables['block_listing'][$region][$i]->block_title =  drupal_render($block['info']);
      $variables['block_listing'][$region][$i]->region_select = drupal_render($block['region']) . drupal_render($block['theme']);
      $variables['block_listing'][$region][$i]->weight_select = drupal_render($block['weight']);
      $variables['block_listing'][$region][$i]->configure_link = drupal_render($block['configure']);
      $variables['block_listing'][$region][$i]->delete_link = !empty($block['delete']) ? drupal_render($block['delete']) : '';
      $variables['block_listing'][$region][$i]->printed = FALSE;

      $last_region = $region;
    }
  }

  $variables['form_submit'] = drupal_render_children($variables['form']);
}
