import cv2
import time
import psutil
import matplotlib.pyplot as plt
import numpy as np
import torch
from ultralytics import YOLO

# --- Configurations ---
video_path = "Test_video.mp4"       # Path to your video
model_path = "MODEL_PATH.pt"           # Path to your YOLOv8 model (can be custom-trained)


# --- Load YOLO model ---
model = YOLO(model_path)
model.eval()  # Set model to evaluation mode
# --- Open video ---
cap = cv2.VideoCapture(video_path)
if not cap.isOpened():
    print("Error: Cannot open video.")
    exit()

# --- Data collection ---
frame_times = []
cpu_usages = []
ram_usages = []
frame_fps = []

# --- Frame loop ---
start_total_time = time.time()
while True:
    t_start = time.time()

    ret, frame = cap.read()
    if not ret:
        break

    # --- Inference ---
    results = model(frame, verbose=False, conf=0.85, iou=0.5)
    annotated_frame = results[0].plot()

    t_end = time.time()

    # --- Timing stats ---
    duration = t_end - t_start
    frame_times.append(duration)
    frame_fps.append(1.0 / duration if duration > 0 else 0)

    # --- System load stats ---
    cpu_usages.append(psutil.cpu_percent(interval=None))  # Current CPU usage %
    ram_usages.append(psutil.virtual_memory().percent)    # RAM usage %

    # --- Display ---
    cv2.imshow("YOLO Inference", annotated_frame)

    # Press 'q' to quit
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# --- Cleanup ---
cap.release()
cv2.destroyAllWindows()

# --- Calculate summary stats ---
end_total_time = time.time()
total_elapsed_time = end_total_time - start_total_time
total_time = sum(frame_times)
num_frames = len(frame_times)
avg_fps = num_frames / total_time if total_time > 0 else 0

print(f"\n=== Timing Summary ===")
print(f"Processed {num_frames} frames")
print(f"Total elapsed time (real-time): {total_elapsed_time:.2f}s")
print(f"Total inference time: {total_time:.2f}s")
print(f"Overhead time (display, I/O, etc.): {total_elapsed_time - total_time:.2f}s")
print(f"Average FPS (inference only): {avg_fps:.2f}")
print(f"Average FPS (real-time): {num_frames / total_elapsed_time:.2f}")

# --- Plot inference timing ---
frame_numbers = list(range(1, num_frames + 1))
frame_times_ms = [t * 1000 for t in frame_times]  # convert to ms

# Rolling average
window_size = 10
rolling_avg = np.convolve(frame_times_ms, np.ones(window_size)/window_size, mode='valid')
rolling_frame_numbers = list(range(window_size, len(frame_times_ms) + 1))

plt.figure(figsize=(14, 7))
plt.plot(frame_numbers, frame_times_ms, label='Per-frame Time (ms)', color='blue', alpha=0.6)
plt.plot(rolling_frame_numbers, rolling_avg, label=f'{window_size}-frame Rolling Avg (ms)', color='orange', linewidth=2)
plt.plot(frame_numbers, frame_fps, label='Per-frame FPS', color='green', linestyle='--', alpha=0.5)
plt.title('YOLO Inference Timing Analysis')
plt.xlabel('Frame Number')
plt.ylabel('Time / FPS')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("frame_timing_plot.png")
plt.show()

# --- Plot system load ---
plt.figure(figsize=(14, 6))
plt.plot(frame_numbers, cpu_usages, label='CPU Usage (%)', color='red')
plt.plot(frame_numbers, ram_usages, label='RAM Usage (%)', color='purple')
plt.title('System Load During YOLO Inference')
plt.xlabel('Frame Number')
plt.ylabel('Usage (%)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("system_load_plot.png")
plt.show()