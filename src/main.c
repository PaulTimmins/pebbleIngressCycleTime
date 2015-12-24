#include <pebble.h>

static Window *my_window;
TextLayer *tl_cycle;
TextLayer *tl_cp;
TextLayer *tl_countdown;
TextLayer *tl_list;
TextLayer *tl_conn_layer;
TextLayer *tl_batt_layer;
TextLayer *tl_realtime;
TextLayer *tl_region_layer;
GBitmap *img_res;
BitmapLayer *bl_res;

static char region[] = "-REGION-";
static uint32_t mydata = 0; //you want ants? That's how you get ants.
static uint32_t lastcheck = 0; //last time we asked the phone for region/tz data

static uint32_t updatingtime = 0;

#define START_TIME_SEC 0  //checkpoints actually can be done without special modulus - unix epoch is okay
#define CYCLE_SEC 630000
#define CP_SEC 18000
#define BUF_SIZE 30
#define SHOW_CP_NUM 3 // number of checkpoints displayed
#define CP_DATA_SIZE 6 // number of characters per checkpoint

static char buffer[7][BUF_SIZE];
//char battstate[BUF_SIZE];
static char tickstate;

static void handle_tick(struct tm *tick_time, TimeUnits units_changed);
static void appmessage_init(void);
static void main_window_load(Window *window);
static void main_window_unload(Window *window);

char *translate_error(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

static void handle_conn(bool connected) {
    //APP_LOG( APP_LOG_LEVEL_ERROR , "connected toggle");
	if (connected) {
    #ifdef PBL_COLOR
       text_layer_set_background_color(tl_conn_layer, GColorBlue);
    #else 
       text_layer_set_background_color(tl_conn_layer, GColorBlack);
    #endif
      //text_layer_set_background_color(tl_conn_layer, GColorBlack);
      text_layer_set_text_color(tl_conn_layer, GColorWhite);
      text_layer_set_text(tl_conn_layer, "BT: OK");
      //vibes_short_pulse();
	} else {
       #ifdef PBL_COLOR
          //text_layer_set_background_color(tl_conn_layer, GColorBlack);
          text_layer_set_background_color(tl_conn_layer, GColorBlue);
          text_layer_set_text_color(tl_conn_layer, GColorRed);
       #else
          text_layer_set_background_color(tl_conn_layer, GColorWhite);
          text_layer_set_text_color(tl_conn_layer, GColorBlack);
      #endif
			text_layer_set_text(tl_conn_layer, "BT: LOST");
      vibes_long_pulse();
	}
}

static void handle_batt(BatteryChargeState charge) {
   if (updatingtime) {
    APP_LOG( APP_LOG_LEVEL_ERROR , "called handle_batt UPDATING TIME ALREADY aieee");
    return;
  };
        APP_LOG (APP_LOG_LEVEL_ERROR , "battery event old buffer (%s)",buffer[6]);
   #ifdef PBL_COLOR

          //text_layer_set_background_color(tl_batt_layer, GColorBlack);
          APP_LOG( APP_LOG_LEVEL_ERROR , "white");
   
          text_layer_set_text_color(tl_batt_layer, GColorWhite);
          APP_LOG( APP_LOG_LEVEL_ERROR , "set to white");
            if (charge.charge_percent <= 20)  {
                APP_LOG( APP_LOG_LEVEL_ERROR , "red");
                text_layer_set_text_color(tl_batt_layer, GColorRed);
            }
            if ((charge.charge_percent <= 50) && (charge.charge_percent > 20))  {
                APP_LOG( APP_LOG_LEVEL_ERROR , "pastelyellow");
                text_layer_set_text_color(tl_batt_layer, GColorPastelYellow);
            }
            if (charge.is_charging)  {
                APP_LOG( APP_LOG_LEVEL_ERROR , "normalyellow");
                text_layer_set_text_color(tl_batt_layer, GColorYellow);
            }
            if (charge.charge_percent >= 90) {
                APP_LOG( APP_LOG_LEVEL_ERROR , "green");
                text_layer_set_text_color(tl_batt_layer, GColorGreen);
            }
  #endif
  
  APP_LOG( APP_LOG_LEVEL_ERROR , "before snprintf %s: %d%%",charge.is_charging?"CHG":"BATT",charge.charge_percent);
  snprintf(buffer[6], sizeof(buffer[6]), "%s: %d%%", charge.is_charging?"CHG":"BATT",charge.charge_percent);
  APP_LOG( APP_LOG_LEVEL_ERROR , "after snprintf (%s) handle batt", buffer[6]);
  //text_layer_set_text(tl_batt_layer, buffer[6]);
  APP_LOG( APP_LOG_LEVEL_ERROR , "after textlayer (%s) handle batt", buffer[6]);
}



static void update_time(bool fullupdate) {
  if (updatingtime) {
    APP_LOG( APP_LOG_LEVEL_ERROR , "called update_time UPDATING TIME ALREADY aieee");
    return;
  };
  updatingtime=1;
  APP_LOG(APP_LOG_LEVEL_ERROR , "updating time");
    
  int32_t utcoffset;
  utcoffset =  mydata - 2400;
   APP_LOG( APP_LOG_LEVEL_ERROR , "%ld: utcoffset tick",utcoffset*60);
	time_t rt = time(NULL);
  uint32_t t = rt;
  uint32_t countdown;
  if (clock_is_timezone_set()) { //thanks, pebble. #sigh
    countdown = CP_SEC - t  % CP_SEC ;  
    } else {
	  countdown = CP_SEC - (t + utcoffset*60)  % CP_SEC ;
  };
	struct tm *tms;
  struct tm *acttime;
  struct tm *curcyclestart;
  
	if(!fullupdate){
    //APP_LOG( APP_LOG_LEVEL_ERROR , "not full update");
		fullupdate = (rt % 3600 == 0);
    //if (lastcheck > 0 && lastcheck+500 < t) { APP_LOG( APP_LOG_LEVEL_ERROR, "lastcheck %ld, fullupdate %d, rt %ld polling handset for update",lastcheck,fullupdate,rt);   lastcheck = (uint32_t) time(NULL); app_message_outbox_send(); };
	}
	
	if(fullupdate){

      uint32_t next;
      uint32_t last;
      if (clock_is_timezone_set()) { //thanks, pebble. #sigh
      next = rt + countdown;
      last = rt - (CYCLE_SEC-countdown);
      } else {
      next = rt + countdown + utcoffset*60;
      last = rt - (CYCLE_SEC-countdown)+utcoffset*60;
      }
      curcyclestart = localtime((time_t*) &last);
		  uint32_t year = curcyclestart->tm_year+1900; //we have our year
      uint32_t offset = last-(curcyclestart->tm_yday*86400)-(curcyclestart->tm_hour*60*60)-(curcyclestart->tm_min*60)-(curcyclestart->tm_sec);
      uint32_t cycle;
      uint32_t cp;

        cycle = (t - offset) / CYCLE_SEC;
        cp = (t % CYCLE_SEC) / CP_SEC + 1;
      
      APP_LOG( APP_LOG_LEVEL_ERROR , "full update %ld: utcoffset, %lu: offset, %lu: cycle, %lu: checkpoint, %lu: year", utcoffset, offset, cycle, cp, year);

		  snprintf(buffer[0], BUF_SIZE, "%lu.%02lu", year, cycle); 
		  snprintf(buffer[1], BUF_SIZE, "%02lu/35", cp);
		  text_layer_set_text(tl_cycle, buffer[0]);
		  text_layer_set_text(tl_cp, buffer[1]);
      if (clock_is_timezone_set()) { //thanks, pebble. #sigh
        tms = localtime((time_t*) &next);
      } else {
        next = next - utcoffset*60;
        tms = localtime((time_t*) &next);
      }
		  int nc = tms->tm_hour;
      int ncm = tms->tm_min;
		  for(int i=0; i<SHOW_CP_NUM; ++i, nc = (nc+5) % 24){
		  snprintf(buffer[3] + CP_DATA_SIZE*i, BUF_SIZE, "%02d:%02d,", nc,ncm); 
		  }
		  buffer[3][SHOW_CP_NUM * CP_DATA_SIZE - 1] = '\0';
		  text_layer_set_text(tl_list, buffer[3]);
    
	}
	tms = gmtime((time_t*)&countdown);
  if (countdown <= 300) {
    strftime(buffer[2], BUF_SIZE, "%H:%M:%S", tms);
    //tick_timer_service_unsubscribe();
    if (tickstate == MINUTE_UNIT) {
    tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
    APP_LOG( APP_LOG_LEVEL_ERROR , "ticks set to seconds");
    tickstate = SECOND_UNIT;
    };
  } else {
    //tick_timer_service_unsubscribe();
    if (tickstate == SECOND_UNIT) {
    tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
    APP_LOG( APP_LOG_LEVEL_ERROR , "ticks set to minutes");
    tickstate = MINUTE_UNIT;
    };
	  strftime(buffer[2], BUF_SIZE, "%H:%M", tms);
  };
    text_layer_set_text(tl_countdown, buffer[2]);
  acttime = localtime((time_t*) &rt);
  strftime(buffer[5], BUF_SIZE, "%m/%d %H:%M", acttime);
  text_layer_set_text(tl_realtime, buffer[5]);
  APP_LOG(APP_LOG_LEVEL_ERROR , "updating time complete");
  //text_layer_set_text(tl_batt_layer, buffer[6]);
  APP_LOG(APP_LOG_LEVEL_ERROR , "writing battery buffer");
  updatingtime=0;
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
    //APP_LOG( APP_LOG_LEVEL_ERROR , "begin tick");
    update_time(false);
    //APP_LOG( APP_LOG_LEVEL_ERROR , "end tick");

}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  //APP_LOG( APP_LOG_LEVEL_ERROR , "inbound data");
	Tuple *ofs = dict_find(iter, 0);
	Tuple *rgn = dict_find(iter, 1);
  if (ofs->value->uint32) {
    if (ofs->value->uint32 != mydata) {
        persist_write_int(1,ofs->value->uint32); //store this for later otherwise don't
        APP_LOG (APP_LOG_LEVEL_ERROR , "%lu: utcoffset stored in slot 1", ofs->value->uint32);
    }
	memcpy(&mydata,&ofs->value->uint32,sizeof(uint32_t));
	strncpy(region, rgn->value->cstring, rgn->length);
  APP_LOG( APP_LOG_LEVEL_ERROR , "%lu: utcoffset received, %s: region",mydata,region);
  text_layer_set_text(tl_region_layer, region);
  lastcheck = (uint32_t) time(NULL);
  update_time(true);
  //app_message_outbox_send();
  };
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "In dropped: %i - %s", reason, translate_error(reason));
}

static void appmessage_init(void) {
	app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
 
	//app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum()); //because you weren't doing anything with that heap, right?
  app_message_open(128,128); //god willing this really should be long enough *fingers crossed*
  app_message_outbox_send();
}

static void handle_init(void) {
	my_window = window_create();
  window_set_window_handlers(my_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
	window_stack_push(my_window, true);

};

static void main_window_unload(Window *window) {
    //nothing
};

static void main_window_load(Window *window) {
	//GFont font_s = fonts_get_system_font(FONT_KEY_GOTHIC_18);
	//GFont font_m = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  //GFont font_24 = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
	//GFont font_b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
	Layer *root_layer = window_get_root_layer(window);
	GRect frame = layer_get_frame(root_layer);
	
  #ifdef PBL_COLOR
    window_set_background_color(window, GColorBlue);
  #else
    window_set_background_color(window, GColorBlack);
  #endif
	
	tl_cycle = text_layer_create(GRect(0, 0, 70, 26));
	text_layer_set_font(tl_cycle, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(tl_cycle, GTextAlignmentLeft);
  #ifdef PBL_COLOR
    text_layer_set_background_color(tl_cycle, GColorBlue);
    text_layer_set_text_color(tl_cycle, GColorWhite);
  #else
    text_layer_set_background_color(tl_cycle, GColorBlack);
    text_layer_set_text_color(tl_cycle, GColorWhite);
	#endif
	
	tl_cp = text_layer_create(GRect(70, 0, frame.size.w-70, 26));
	text_layer_set_font(tl_cp, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(tl_cp, GTextAlignmentRight);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_cp, GColorBlue);
    text_layer_set_text_color(tl_cp, GColorWhite);
  #else
    text_layer_set_background_color(tl_cp, GColorBlack);
    text_layer_set_text_color(tl_cp, GColorWhite);
	#endif
	

	tl_countdown = text_layer_create(GRect(0, 25, frame.size.w, 30));
	text_layer_set_font(tl_countdown, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(tl_countdown, GTextAlignmentCenter);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_countdown, GColorBlue);
    text_layer_set_text_color(tl_countdown, GColorWhite);
  #else
    text_layer_set_background_color(tl_countdown, GColorBlack);
    text_layer_set_text_color(tl_countdown, GColorWhite);
	#endif
	
	tl_list = text_layer_create(GRect(0, frame.size.h-25, frame.size.w, 20));
	text_layer_set_font(tl_list, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(tl_list, GTextAlignmentCenter);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_list, GColorBlue);
    text_layer_set_text_color(tl_list, GColorWhite);
  #else
    text_layer_set_background_color(tl_list, GColorBlack);
    text_layer_set_text_color(tl_list, GColorWhite);
	#endif
	
	
  img_res = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RES); //48x67
  bl_res = bitmap_layer_create(GRect(0, 60, 50, 70));
  bitmap_layer_set_bitmap(bl_res, img_res);
  bitmap_layer_set_alignment(bl_res, GAlignLeft); 

#ifdef PBL_COLOR
  bitmap_layer_set_background_color(bl_res, GColorBlue);
#else
  bitmap_layer_set_background_color(bl_res, GColorBlack);
#endif
    
  tl_realtime = text_layer_create(GRect(55, 60, frame.size.w-60, 20));
	text_layer_set_font(tl_realtime, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(tl_realtime, GTextAlignmentRight);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_realtime, GColorBlue);
    text_layer_set_text_color(tl_realtime, GColorWhite);
  #else
    text_layer_set_background_color(tl_realtime, GColorBlack);
    text_layer_set_text_color(tl_realtime, GColorWhite);
	#endif
    
	tl_conn_layer = text_layer_create(GRect(55, 80, frame.size.w-60, 20));
  text_layer_set_font(tl_conn_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(tl_conn_layer, GTextAlignmentRight);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_conn_layer, GColorBlue);
    text_layer_set_text_color(tl_conn_layer, GColorWhite);
  #else
    text_layer_set_background_color(tl_conn_layer, GColorBlack);
    text_layer_set_text_color(tl_conn_layer, GColorWhite);
	#endif
	text_layer_set_text(tl_conn_layer, "BT: --");
	
  tl_batt_layer = text_layer_create(GRect(55, 100, frame.size.w-60, 20));
  text_layer_set_font(tl_batt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(tl_batt_layer, GTextAlignmentRight);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_batt_layer, GColorBlue);
    text_layer_set_text_color(tl_batt_layer, GColorWhite);
  #else
    text_layer_set_background_color(tl_batt_layer, GColorBlack);
    text_layer_set_text_color(tl_batt_layer, GColorWhite);
	#endif
	snprintf(buffer[6],sizeof(buffer[6]),"BATT: --");
  text_layer_set_text(tl_batt_layer, buffer[6]);
 	
	tl_region_layer = text_layer_create(GRect(30, 120, frame.size.w-35, 20));
 	text_layer_set_font(tl_region_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(tl_region_layer, GTextAlignmentRight);
	#ifdef PBL_COLOR
    text_layer_set_background_color(tl_region_layer, GColorBlue);
    text_layer_set_text_color(tl_region_layer, GColorWhite);
  #else
    text_layer_set_background_color(tl_region_layer, GColorBlack);
    text_layer_set_text_color(tl_region_layer, GColorWhite);
	#endif
	text_layer_set_text(tl_region_layer, "-REGION-");
 
	layer_add_child(root_layer, bitmap_layer_get_layer(bl_res));
	layer_add_child(root_layer, text_layer_get_layer(tl_cycle));	
	layer_add_child(root_layer, text_layer_get_layer(tl_cp));
	layer_add_child(root_layer, text_layer_get_layer(tl_countdown));
	layer_add_child(root_layer, text_layer_get_layer(tl_list));
	layer_add_child(root_layer, text_layer_get_layer(tl_conn_layer));
	layer_add_child(root_layer, text_layer_get_layer(tl_batt_layer));
  layer_add_child(root_layer, text_layer_get_layer(tl_region_layer));
	layer_add_child(root_layer, text_layer_get_layer(tl_realtime));
  
	//tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
	bluetooth_connection_service_subscribe(handle_conn);
	handle_conn(bluetooth_connection_service_peek());
	battery_state_service_subscribe(handle_batt);
	handle_batt(battery_state_service_peek());
  mydata=persist_read_int(1); // read in the last known offset
  APP_LOG (APP_LOG_LEVEL_ERROR , "%lu: utcoffset retrieved from slot 1", mydata);
  appmessage_init();
  tickstate=SECOND_UNIT;
  tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
}


static void handle_deinit(void) {
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	//text_layer_destroy(tl_cycle);
	//text_layer_destroy(tl_cp);
	//text_layer_destroy(tl_countdown);
	//text_layer_destroy(tl_list);
	//text_layer_destroy(tl_conn_layer);
	//text_layer_destroy(tl_batt_layer);
 	gbitmap_destroy(img_res);
	bitmap_layer_destroy(bl_res);
	window_destroy(my_window);
}

int main(void) {
	handle_init();
	app_event_loop();
	handle_deinit();
}
