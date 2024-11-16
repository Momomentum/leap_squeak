#include "ExampleConnection.h"
#include <LeapC.h>
#include <cmath>
#include <iostream>
#include <libremidi/libremidi.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

static libremidi::midi_out midi_out;
static LEAP_CONNECTION *connectionHandle;
template <typename T>

static auto are_equal(T f1, T f2) -> bool {
    return (std::fabs(f1 - f2) <= std::numeric_limits<T>::epsilon() * std::fmax(std::fabs(f1), std::fabs(f2)));
}
/** Callback for when the connection opens. */
static void OnConnect(void) { printf("Connected.\n"); }

/** Callback for when a device is found. */
static void OnDevice(const LEAP_DEVICE_INFO *props) { printf("Found device %s.\n", props->serial); }

static int last_note = -1;
static int octave = 1;

static void midi_note_on(int note) {
    if (last_note == note) {
        return;
    }
    midi_out.send_message(0x80, last_note, 0);
    midi_out.send_message(0x90, note, 127);
    last_note = note;
    printf("Note on: %d\n", note);
}

static void midi_note_off(int note) {
    midi_out.send_message(0x80, note, 0); 
    last_note = -1;
}

auto trigger_right_hand(LEAP_HAND *hand) -> void {
    auto pd = hand->pinch_strength;
    if (pd > 0.8) {
        const float num_notes = 12;
        const int offset = 24 + octave * 12;
        int note = std::min(static_cast<int>(hand->palm.position.y * 0.3175), 127) * (num_notes / 127) + offset;
        midi_note_on(note);
    } else {
        if (last_note > -1) {
            midi_note_off(last_note);
            printf("Note off\n");
        }
    }
}

auto trigger_left_hand(LEAP_HAND *hand) -> void {
    octave = 3 * static_cast<int>(hand->palm.position.y) / 400 + 1;
}

/** Callback for when a frame of tracking data is available. */
static void OnFrame(const LEAP_TRACKING_EVENT *frame) {
    bool has_right_hand = false;
    for (uint32_t h = 0; h < frame->nHands; h++) {
        LEAP_HAND *hand = &frame->pHands[h];
        if (hand->type == eLeapHandType_Right) {
            trigger_right_hand(hand);
            has_right_hand = true;
        } else {
            trigger_left_hand(hand);
        } 
        
        // printf("    Hand id %i is a %s hand with position (%f, %f, %f).\n", hand->id,
        // (hand->type == eLeapHandType_Left ? "left" : "right"), hand->palm.position.x, hand->palm.position.y,
        // hand->palm.position.z);
    }

    if (!has_right_hand && last_note > -1) {
      midi_note_off(last_note);
    }
}

static void OnImage(const LEAP_IMAGE_EVENT *image) {
    printf("Image %lli  => Left: %d x %d (bpp=%d), Right: %d x %d (bpp=%d)\n", (long long int)image->info.frame_id,
           image->image[0].properties.width, image->image[0].properties.height, image->image[0].properties.bpp * 8,
           image->image[1].properties.width, image->image[1].properties.height, image->image[1].properties.bpp * 8);
}

static void OnLogMessage(const eLeapLogSeverity severity, const int64_t timestamp, const char *message) {
    const char *severity_str;
    switch (severity) {
    case eLeapLogSeverity_Critical:
        severity_str = "Critical";
        break;
    case eLeapLogSeverity_Warning:
        severity_str = "Warning";
        break;
    case eLeapLogSeverity_Information:
        severity_str = "Info";
        break;
    default:
        severity_str = "";
        break;
    }
    printf("[%s][%lli] %s\n", severity_str, (long long int)timestamp, message);
}

static void *allocate(uint32_t size, eLeapAllocatorType typeHint, void *state) {
    void *ptr = malloc(size);
    return ptr;
}

static void deallocate(void *ptr, void *state) {
    if (!ptr)
        return;
    free(ptr);
}

void OnPointMappingChange(const LEAP_POINT_MAPPING_CHANGE_EVENT *change) {
    if (!connectionHandle)
        return;

    uint64_t size = 0;
    if (LeapGetPointMappingSize(*connectionHandle, &size) != eLeapRS_Success || !size)
        return;

    LEAP_POINT_MAPPING *pointMapping = (LEAP_POINT_MAPPING *)malloc((size_t)size);
    if (!pointMapping)
        return;

    if (LeapGetPointMapping(*connectionHandle, pointMapping, &size) == eLeapRS_Success && pointMapping->nPoints > 0) {
        printf("Managing %u points as of frame %lld at %lld\n", pointMapping->nPoints,
               (long long int)pointMapping->frame_id, (long long int)pointMapping->timestamp);
    }
    free(pointMapping);
}

void OnHeadPose(const LEAP_HEAD_POSE_EVENT *event) {
    printf("Head pose:\n");
    printf("    Head position (%f, %f, %f).\n", event->head_position.x, event->head_position.y, event->head_position.z);
    printf("    Head orientation (%f, %f, %f, %f).\n", event->head_orientation.w, event->head_orientation.x,
           event->head_orientation.y, event->head_orientation.z);
    printf("    Head linear velocity (%f, %f, %f).\n", event->head_linear_velocity.x, event->head_linear_velocity.y,
           event->head_linear_velocity.z);
    printf("    Head angular velocity (%f, %f, %f).\n", event->head_angular_velocity.x, event->head_angular_velocity.y,
           event->head_angular_velocity.z);
}

auto main(int argc, char *argv[]) -> int {
    midi_out.open_virtual_port("test");

    // midi_out.send_message(0x90, 60, 127);
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    // midi_out.send_message(0x80, 60, 0);

    ConnectionCallbacks.on_connection = &OnConnect;
    ConnectionCallbacks.on_device_found = &OnDevice;
    ConnectionCallbacks.on_frame = &OnFrame;
    // ConnectionCallbacks.on_image = &OnImage;
    ConnectionCallbacks.on_point_mapping_change = &OnPointMappingChange;
    ConnectionCallbacks.on_log_message = &OnLogMessage;
    ConnectionCallbacks.on_head_pose = &OnHeadPose;

    connectionHandle = OpenConnection();
    {
        LEAP_ALLOCATOR allocator = {allocate, deallocate, NULL};
        LeapSetAllocator(*connectionHandle, &allocator);
    }
    LeapSetPolicyFlags(*connectionHandle, eLeapPolicyFlag_Images | eLeapPolicyFlag_MapPoints, 0);

    // TODO better infinite loop
    while (true) {}
    CloseConnection();
    DestroyConnection();

    return 0;
}
