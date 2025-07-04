# @author : Nicolas Koch
import asyncio
async def ultrasonic_data():
    process = await asyncio.create_subprocess_exec(
        "../sensors/ultraschall/ultraschall",
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
            print(f"Ultrasonic output: {decoded}") # DEBUG
            yield decoded
    finally:
        process.kill()