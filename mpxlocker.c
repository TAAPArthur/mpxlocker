#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <unistd.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define THRESHOLD 500

xcb_connection_t* dis;
xcb_window_t root;

int16_t masterDevices[255][2];

void grabMaster(int id) {
    xcb_input_grab_device_unchecked(dis,root, XCB_CURRENT_TIME, 0, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 1, id, NULL);
}

void grabNewMaster(xcb_input_hierarchy_event_t* event) {
    xcb_input_hierarchy_info_iterator_t iter = xcb_input_hierarchy_infos_iterator(event);
    while(iter.rem) {
        if(iter.data->flags & XCB_INPUT_HIERARCHY_MASK_MASTER_ADDED) {
            grabMaster(iter.data->deviceid);
        }
        xcb_input_hierarchy_info_next(&iter);
    }
}

void grabAllMasters() {
    xcb_input_xi_query_device_cookie_t cookie=xcb_input_xi_query_device(dis, XCB_INPUT_DEVICE_ALL_MASTER);
    xcb_input_xi_query_device_reply_t *reply=xcb_input_xi_query_device_reply(dis, cookie, NULL);
    xcb_input_xi_device_info_iterator_t  iter= xcb_input_xi_query_device_infos_iterator(reply);
    while(iter.rem){
        xcb_input_xi_device_info_t* info = iter.data;
        grabMaster(info->deviceid);
        xcb_input_xi_device_info_next (&iter);
    }
    free(reply);
}


void saveStartPosition(xcb_input_button_press_event_t*  event) {
    masterDevices[event->deviceid][0] = event->root_x >> 16;
    masterDevices[event->deviceid][1] = event->root_y >> 16;
}

int isDisplacementGreaterThanThreshold(xcb_input_button_press_event_t*  event) {
    int deltaX = abs(masterDevices[event->deviceid][0] - (event->root_x >> 16));
    int deltaY = abs(masterDevices[event->deviceid][1] - (event->root_y >> 16));
    return deltaX > THRESHOLD || deltaY > THRESHOLD;
}

void selectEvents(int eventMask) {
    struct {
		xcb_input_event_mask_t head;
		xcb_input_xi_event_mask_t mask;
	} mask = {
        {XCB_INPUT_DEVICE_ALL, sizeof(xcb_input_xi_event_mask_t) / sizeof(uint32_t)}, eventMask};
    xcb_input_xi_select_events(dis, root, 1, &mask.head);
    xcb_flush(dis);
}

void usage() {
    printf("mpxlocker [-hn] [id1] [id2] ...\n");
}

void processArgs(const char** argv) {
    int eventMask = XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS | XCB_INPUT_XI_EVENT_MASK_BUTTON_RELEASE | XCB_INPUT_XI_EVENT_MASK_HIERARCHY;
    int i = 1;
    for(; *argv && argv[0][0] == '-'; argv++) {
        if(argv[i][1] == '-')
            break;
        switch(argv[i][0]){
            case 'h':
                usage();
                exit(0);
            case 'n':
                eventMask &= ~XCB_INPUT_XI_EVENT_MASK_HIERARCHY;
                break;
        }
    }
    selectEvents(eventMask);
    if(*argv) {
        for(; *argv; argv++)
            grabMaster(atoi(*argv));
    }
    else {
       grabAllMasters();
    }
    xcb_flush(dis);
}

int main(int argc, const char* argv[]) {
    dis = xcb_connect(NULL, NULL);
    root = xcb_setup_roots_iterator(xcb_get_setup(dis)).data->root;
    processArgs(argv + 1);

    xcb_generic_event_t* event;
    while((event = xcb_wait_for_event(dis))) {
        int type = event->response_type & 127;
        if (type == XCB_GE_GENERIC) {
            if(((xcb_ge_generic_event_t*)event)->extension == xcb_get_extension_data(dis, &xcb_input_id)->major_opcode) {
                switch(((xcb_ge_generic_event_t*)event)->event_type) {
                    case XCB_INPUT_HIERARCHY :
                        grabNewMaster((void*)event);
                        break;
                    case XCB_INPUT_BUTTON_PRESS:
                        saveStartPosition((void*)event);
                        break;
                    case XCB_INPUT_BUTTON_RELEASE:
                        if(isDisplacementGreaterThanThreshold((void*)event))
                            exit(0);
                }
            }
        }
        free(event);
    }
}
