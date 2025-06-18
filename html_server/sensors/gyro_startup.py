import asyncio
import subprocess

async def gyro_data():
    process = await asyncio.create_subprocess_exec(
        "../gyro-filter/gyro_kalman",
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL
    )

    try:
        while True:
            line = await process.stdout.readline()
            if not line:
                break
            yield line.decode().strip()
    finally:
        process.kill()
