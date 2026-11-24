import sys
import json

buffer = ""

for line in sys.stdin:
    buffer += line

    decoder = json.JSONDecoder()

    while buffer:
        buffer = buffer.lstrip()

        if not buffer:
            break

        try:
            obj, end = decoder.raw_decode(buffer)

            print(json.dumps(obj, indent=2), flush=True)
            buffer = buffer[end:]

        except json.JSONDecodeError:
            break
