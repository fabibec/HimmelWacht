import subprocess
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import math

# start C++ program
proc = subprocess.Popen(["./gyro_kalman.cpp"], stdout=subprocess.PIPE, text=True)

fig, ax = plt.subplots()
line, = ax.plot([], [], lw=3)
ax.set_xlim(-1, 1)
ax.set_ylim(-1, 1)
ax.set_aspect('equal')
ax.grid()

def init():
    line.set_data([], [])
    return line,

def update(frame):
    global proc
    try:
        line_out = proc.stdout.readline()
        if not line_out:
            return line,

        pitch, roll = map(float, line_out.strip().split(", "))

        length = 0.8
        x = length * math.sin(math.radians(roll))
        y = length * math.sin(math.radians(pitch))
        line.set_data([0, x], [0, y])
        ax.set_title(f"Pitch: {pitch:.1f}°, Roll: {roll:.1f}°")
    except:
        pass
    return line,

ani = animation.FuncAnimation(fig, update, init_func=init, interval=100, blit=True)
plt.show()

proc.kill()  # kill the C++ process when done
