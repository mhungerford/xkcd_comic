#include <pebble.h>

// PNG support
// Includes Grayscale support for 1 bit (B&W)
#include "upng.h"

//#define max_images 15
static uint8_t max_images = 0;

#define MAX(A,B) ((A>B) ? A : B)
#define MIN(A,B) ((A<B) ? A : B)
#define LEFT '1'

// Necessary to modify frame directly, as layer_set_frame causes
// layer_mark_dirty, which before each animation causes flicker
typedef struct myLayer {
  GRect bounds;
  GRect frame;
} myLayer;

//Register stack pointer to print it (needs -ffixed-sp in CFLAGS also)
register uint32_t sp __asm("sp");
static uint32_t bsp = 0x2001a26c; //stack grows downward from this

void flip_byte(uint8_t* byteval) {
  uint8_t v = *byteval;
  uint8_t r = v; // r will be reversed bits of v; first get LSB of v
  int s = 7; // extra shift needed at end

  for (v >>= 1; v; v >>= 1) {   
    r <<= 1;
    r |= v & 1;
    s--;
  }
  r <<= s; // shift when v's highest bits are zero
  *byteval = r;
}

static struct main_ui {
  Window* window;
  BitmapLayer* bitmap_layer;
  BitmapLayer* bitmap_layer_old;
  Layer* animation_layer;
  PropertyAnimation* prop_animation_slide_left;
  PropertyAnimation* prop_animation_slide_up;
  AnimationImplementation* animation_implementation;

  GBitmap bitmap;
  GBitmap bitmap_old;
  upng_t* upng;
  uint8_t image_index;
  uint8_t* animation_style_config;
} ui = {
  .bitmap.addr = NULL,
  .bitmap.bounds = {{0},{0}},
  .image_index = 0,
  .prop_animation_slide_left = NULL,
  .prop_animation_slide_up = NULL,
  .animation_style_config = NULL,
  .upng = NULL
};

static void app_exit(int32_t status){
  (*(uint32_t*)NULL) = 0;
}

static bool gbitmap_from_bitmap(
    GBitmap* gbitmap, const uint8_t* bitmap_buffer, int width, int height) {

  //copy current bitmap ptr into old bitmap
  if (gbitmap->addr) {
    ui.bitmap_old = ui.bitmap;
    gbitmap->addr = NULL;
  }

  // Limit PNG to screen size
  width = MIN(width,144);
  height = MIN(height,168);

  // Copy width and height to GBitmap
  gbitmap->bounds.size.w = width;
  gbitmap->bounds.size.h = height;
  // GBitmap needs to be word aligned per line (bytes)
  gbitmap->row_size_bytes = ((width + 31) / 32 ) * 4;
  //Allocate new gbitmap array
  gbitmap->addr = malloc(height * gbitmap->row_size_bytes); 
  if (gbitmap->addr == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "malloc gbitmap->addr failed");
    app_exit(1);
  }

  for(int y = 0; y < height; y++) {
    memcpy(
      &(((uint8_t*)gbitmap->addr)[y * gbitmap->row_size_bytes]), 
      &(bitmap_buffer[y * ((width + 7) / 8)]), 
      (width + 7) / 8);
  }

  // GBitmap pixels are most-significant bit, so need to flip each byte.
  for(int i = 0; i < gbitmap->row_size_bytes * height; i++){
    flip_byte(&((uint8_t*)gbitmap->addr)[i]);
  }

  return true;
}

static void init_animation(void) {
  GRect right_image_bounds = {.origin={.x=144,.y=0},.size={.w=144,.h=168}};
  GRect left_image_bounds = {.origin={.x=0,.y=0},.size={.w=144,.h=168}};

  GRect top_image_bounds = {.origin={.x=0,.y=0},.size={.w=144,.h=168}};
  GRect bottom_image_bounds = {.origin={.x=0,.y=168},.size={.w=144,.h=168}};

  // Setup slide left animation
  ui.prop_animation_slide_left = property_animation_create_layer_frame( 
    ui.animation_layer,
    &right_image_bounds, &left_image_bounds);

  animation_set_duration((Animation*)ui.prop_animation_slide_left, 1000);
  animation_set_curve((Animation*)ui.prop_animation_slide_left, 
    AnimationCurveEaseInOut);

  // Setup slide up animation
  ui.prop_animation_slide_up = property_animation_create_layer_frame( 
    ui.animation_layer,
    &bottom_image_bounds, &top_image_bounds);

  animation_set_duration((Animation*)ui.prop_animation_slide_up, 1000);
  animation_set_curve((Animation*)ui.prop_animation_slide_up, 
    AnimationCurveEaseInOut);
}


static bool load_png_resource(int index) {
  ResHandle rHdl = resource_get_handle(RESOURCE_ID_IMAGE_1 + ui.image_index);
  int png_raw_size = resource_size(rHdl);
    
  // Free the old bitmap here to have the memory available for decompression
  if (ui.bitmap_old.addr) {
    free(ui.bitmap_old.addr);
  }
  
  uint8_t* png_raw_buffer = malloc(png_raw_size); //freed by upng impl
  if (png_raw_buffer == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "malloc png resource buffer failed");
    app_exit(1);
  }
  resource_load(rHdl, png_raw_buffer, png_raw_size);
  ui.upng = upng_new_from_bytes(png_raw_buffer, png_raw_size);
  if (ui.upng == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG malloc error"); 
    app_exit(1);
  }
  if (upng_get_error(ui.upng) != UPNG_EOK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Loaded:%d line:%d", 
      upng_get_error(ui.upng), upng_get_error_line(ui.upng));
    app_exit(1);
  }
  if (upng_decode(ui.upng) != UPNG_EOK) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "UPNG Decode:%d line:%d", 
      upng_get_error(ui.upng), upng_get_error_line(ui.upng));
    app_exit(1);
  }



  gbitmap_from_bitmap(&ui.bitmap, upng_get_buffer(ui.upng),
    upng_get_width(ui.upng), upng_get_height(ui.upng));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "converted to gbitmap");

  // Free the png, no longer needed
  upng_free(ui.upng);
  ui.upng = NULL;

  return true;
}


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Decrement the index (wrap around if negative)
  ui.image_index = ((ui.image_index - 1) < 0)? (max_images - 1) : (ui.image_index - 1);
  load_png_resource(ui.image_index);
  GRect window_bounds = layer_get_bounds(window_get_root_layer(ui.window));
  layer_set_bounds(bitmap_layer_get_layer(ui.bitmap_layer), window_bounds);
  layer_mark_dirty(ui.animation_layer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if((ui.prop_animation_slide_left && 
      !animation_is_scheduled(ui.prop_animation_slide_left))
      || 
      (ui.prop_animation_slide_up && 
      !animation_is_scheduled(ui.prop_animation_slide_up))
      ) {
    // Increment the index (wrap around if necessary)
    ui.image_index = (ui.image_index + 1) % max_images;
    load_png_resource(ui.image_index);
    if (ui.animation_style_config[ui.image_index] == LEFT) {
      GRect left_image_frame = {.origin={.x=-144,.y=0},.size={.w=144,.h=168}};
      //layer_set_frame(bitmap_layer_get_layer(ui.bitmap_layer_old),left_image_frame);
      ((myLayer*)bitmap_layer_get_layer(ui.bitmap_layer_old))->frame = 
        left_image_frame;

      GRect right_image_frame = {.origin={.x=0,.y=0},.size={.w=144,.h=168}};
      //layer_set_frame(bitmap_layer_get_layer(ui.bitmap_layer),right_image_frame);
      ((myLayer*)bitmap_layer_get_layer(ui.bitmap_layer))->frame = 
        right_image_frame;

      animation_schedule((Animation*)ui.prop_animation_slide_left);
    } else {
      GRect left_image_frame = {.origin={.x=0,.y=-168},.size={.w=144,.h=168}};
      //layer_set_frame(bitmap_layer_get_layer(ui.bitmap_layer_old),left_image_frame);
      ((myLayer*)bitmap_layer_get_layer(ui.bitmap_layer_old))->frame = 
        left_image_frame;

      GRect right_image_frame = {.origin={.x=0,.y=0},.size={.w=144,.h=168}};
      //layer_set_frame(bitmap_layer_get_layer(ui.bitmap_layer),right_image_frame);
      ((myLayer*)bitmap_layer_get_layer(ui.bitmap_layer))->frame = 
        right_image_frame;

      animation_schedule((Animation*)ui.prop_animation_slide_up);
    }
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}


static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "create animation_layer");
  GRect double_wide = {.origin={.x=0,.y=0},.size={.w=144,.h=168}};
  ui.animation_layer = layer_create(double_wide);

  layer_add_child(window_layer, ui.animation_layer);
  
  //layer_set_bounds(ui.animation_layer, bounds);
  layer_set_clips(ui.animation_layer, false);

  
  ui.bitmap_layer_old = bitmap_layer_create(bounds);
  //layer_set_clips(bitmap_layer_get_layer(ui.bitmap_layer_old), false);
  
  ui.bitmap_layer = bitmap_layer_create(bounds);
  //layer_set_clips(bitmap_layer_get_layer(ui.bitmap_layer), false);

  //add old first, so newer image will be on top for back button drawing
  layer_add_child(ui.animation_layer, bitmap_layer_get_layer(ui.bitmap_layer_old));
  layer_add_child(ui.animation_layer, bitmap_layer_get_layer(ui.bitmap_layer));

  load_png_resource(ui.image_index);
  bitmap_layer_set_bitmap(ui.bitmap_layer, &ui.bitmap);
  bitmap_layer_set_bitmap(ui.bitmap_layer_old, &ui.bitmap_old);
  //layer_mark_dirty(bitmap_layer_get_layer(ui.bitmap_layer));

  // Animation
  if (ui.prop_animation_slide_left == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "init animation");
    init_animation();
  }
 
  
}

static void window_unload(Window *window) {
  if (ui.prop_animation_slide_left != NULL) {
    property_animation_destroy(ui.prop_animation_slide_left);
  }
  if (ui.prop_animation_slide_up != NULL) {
    property_animation_destroy(ui.prop_animation_slide_up);
  }
}

static void init(void) {
  //Load the resource animation configuration file
  ResHandle animation_style_config_handle = 
    resource_get_handle(RESOURCE_ID_ANIMATION_CONFIG);
  int animation_style_config_bytes = resource_size(animation_style_config_handle);
  ui.animation_style_config = (uint8_t*)malloc(animation_style_config_bytes);
  if (ui.animation_style_config == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "animation_style_config malloc failed"); 
    app_exit(1);
  }
  
  resource_load(animation_style_config_handle, 
    ui.animation_style_config, animation_style_config_bytes);

  //Discover how many images from base index
  while (resource_get_handle(RESOURCE_ID_IMAGE_1 + max_images)) {
    max_images++;
  }

//APP_LOG(APP_LOG_LEVEL_DEBUG, "Stack Used:%ld SP:%p", bsp - sp, sp);

  light_enable(true);

  ui.window = window_create();
  window_set_fullscreen(ui.window, true);
  //window_set_background_color(ui.window, GColorClear);//Allows compositing
  window_set_click_config_provider(ui.window, click_config_provider);
  window_set_window_handlers(ui.window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = false;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "window push.");
  window_stack_push(ui.window, animated);
}

static void deinit(void) {
  window_destroy(ui.window);
  if (ui.upng) upng_free(ui.upng);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
