#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "LeapC.h"
#include "ExampleConnection.h"

#define DATA_PRINT_INTERVAL 30
#define MAX_FINGER_ANGLE 110.0f // Approx max total angle when finger is fully closed

long long frame_counter = 0;

typedef struct {
  float x, y, z;
} vec3;

vec3 to_vec3(LEAP_VECTOR v) {
  vec3 out = { v.x, v.y, v.z };
  return out;
}

vec3 subtract(vec3 a, vec3 b) {
  vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
  return r;
}

float dot(vec3 a, vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(vec3 v) {
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3 normalize(vec3 v) {
  float len = length(v);
  if (len == 0.0f) return (vec3){0, 0, 0};
  return (vec3){v.x / len, v.y / len, v.z / len};
}

float angle_between(vec3 a, vec3 b) {
  float d = dot(normalize(a), normalize(b));
  if (d < -1.0f) d = -1.0f;
  if (d > 1.0f) d = 1.0f;
  return acosf(d) * (180.0f / 3.14159f); // Return angle in degrees
}

float calculate_finger_curl(LEAP_BONE *bones) {
  vec3 v1 = subtract(to_vec3(bones[2].next_joint), to_vec3(bones[1].prev_joint)); // Proximal to Intermediate
  vec3 v2 = subtract(to_vec3(bones[3].next_joint), to_vec3(bones[2].prev_joint)); // Intermediate to Distal

  float a1 = angle_between(v1, v2);  // Total curl angle (in degrees)

  float open_angle = 5.0f;   // Open finger ~5°
  float closed_angle = 65.0f; // Closed fist ~65°
  float normalized = (a1 - open_angle) / (closed_angle - open_angle);

  if (normalized < 0.0f) normalized = 0.0f;
  if (normalized > 1.0f) normalized = 1.0f;
  return normalized * 100.0f;  // Return curl as percentage
}


static void OnConnect(void) {
  printf("Connected.\n");
}

static void OnDevice(const LEAP_DEVICE_INFO *props) {
  printf("Found device %s.\n", props->serial);
}

static void OnFrame(const LEAP_TRACKING_EVENT *frame) {
  frame_counter++;
  if (frame_counter % DATA_PRINT_INTERVAL == 0) {
    for (uint32_t h = 0; h < frame->nHands; h++) {
      LEAP_HAND *hand = &frame->pHands[h];
      printf("Hand %s\n", hand->type == eLeapHandType_Left ? "Left" : "Right");
      printf("  Palm Position:    (%.2f, %.2f, %.2f)\n", hand->palm.position.x, hand->palm.position.y, hand->palm.position.z);
      printf("  Palm Orientation: (%.2f, %.2f, %.2f)\n", hand->palm.orientation.x, hand->palm.orientation.y, hand->palm.orientation.z);
      printf("  Palm Direction:   (%.2f, %.2f, %.2f)\n", hand->palm.direction.x, hand->palm.direction.y, hand->palm.direction.z);
      printf("  Pinch Distance:   %.2f\n", hand->pinch_distance);
      printf("  Pinch Strength:   %.2f\n", hand->pinch_strength);

      const char* finger_names[5] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

      for (int f = 1; f < 5; f++) { // Skip thumb (f=0), use fingers 1–4
        float curl = calculate_finger_curl(hand->digits[f].bones);
        printf("  %s Finger Curl:    %.1f%%\n", finger_names[f], curl);
      }

      printf("\n");
    }
  }
}

static void OnImage(const LEAP_IMAGE_EVENT *imageEvent) {
  // (Optional) handle image events if needed
}

int main(int argc, char **argv) {
  ConnectionCallbacks.on_connection   = &OnConnect;
  ConnectionCallbacks.on_device_found = &OnDevice;
  ConnectionCallbacks.on_frame        = &OnFrame;
  ConnectionCallbacks.on_image        = &OnImage;

  LEAP_CONNECTION *connection = OpenConnection();
  LeapSetPolicyFlags(*connection, eLeapPolicyFlag_Images, 0);

  printf("Press Enter to exit program.\n");
  getchar();
  
  int pipefd[2];
  pid_t pid;
  char data[] = "nai tu treaba";

  if(pipe(pipefd) == -1){
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if(pid < 0){
    perror("fork");
    exit(EXIT_FAILURE);
  }

    if (pid == 0) {
        // Child process: will execute Python and read from pipefd[0]
        close(pipefd[1]);  // Close write end in child
        // Duplicate read end to stdin (fd 0)
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        // Execute python script
        execlp("python3", "python3", "pipe_reader.py", NULL);
        // If execlp failed
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process: write data to pipe
        close(pipefd[0]);  // Close read end in parent
        write(pipefd[1], data, strlen(data));
        close(pipefd[1]);  // Signal EOF to child
        wait(NULL); // Wait for child to finish
    }


  CloseConnection();
  DestroyConnection();
  return 0;
}
