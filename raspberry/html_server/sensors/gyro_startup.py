import asyncio

async def gyro_data():
    process = await asyncio.create_subprocess_exec(
        "../sensors/gyro-filter/gyro_kalman",
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.DEVNULL
    )

    try:
        while True:
            line = await process.stdout.readline()
            if not line:
                print("Subprocess closed stdout")
                break
            decoded = line.decode().strip()
            print(f"gyro output: {decoded}")
            yield decoded
    finally:
        process.kill()