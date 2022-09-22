import flit
from audioop import maxpp
import whisper
import sounddevice as sd
import types
import numpy as np
import queue
import faulthandler

faulthandler.enable()


fs = 16000 # Whisper sample rate
bindex = 0

class SpeechSegment():
    start_block = 0
    end_block = 0

    def __init__(self, start_block, end_block) -> None:
        self.start_block = start_block
        self.end_block = end_block

speech_confirmed = ""
in_progress_speech_detected = ""
ambience_threshold = 20.0
silence_time = 0.1
active_speech_segment = SpeechSegment(0, 0)
speech_in_progress = False
example_global_var = "Global var!"

tiny_model = whisper.load_model("tiny")
# medium_model = whisper.load_model("medium")

def get_string():
    return example_global_var

downsample = 1
mapping = [0]
block_time = .3
block_size = int(fs*block_time)
q = queue.Queue(maxsize=10)
# block_start_offset = 0
# max_blocks_remembered = 
blocks = []
speech_segments = []
def evaluate_speed(indata, frames, time, status):
    global bindex
    global q
    q.put(indata[::downsample, mapping], block=True)
    bindex += 1

time_chunks = [0.5, 3.0]
rolling_window_time = 30

def update_speech_eval(block):
    global in_progress_speech_detected
    global blocks
    global tiny_model
    # print("Start decoding!\n")
    # print(f"{np.shape(block)}\n")
    # print(f"{active_speech_segment.start_block}\n")
    block_group = np.asarray([past_block for past_block in blocks[int(-rolling_window_time/block_time):]], dtype='f')
    np.append(block, block_group)
    block_segment = block_group.flatten()
    audio = whisper.pad_or_trim(block_segment)
    # print(f"{np.shape(audio)}\n")
    mel = whisper.log_mel_spectrogram(audio).to(tiny_model.device)
    options = whisper.DecodingOptions(language='English')
    result = whisper.decode(tiny_model, mel, options)
    print(result.text)
    in_progress_speech_detected = result.text
    # print(f"SS{len(speech_segments)} blocks{len(blocks)}\n")
    # print("Finished decoding!\n")

# def start_recording():
#     instream = sd.InputStream(samplerate=fs, blocksize=block_size, device=sd.default.device, channels=1, dtype='float32', latency='low', extra_settings=None, callback=evaluate_speed, finished_callback=None, clip_off=None, dither_off=None, never_drop_input=None, prime_output_buffers_using_stream_callback=None)
#     instream.__enter__()
#     print("Recording!\n")

def process_recording():
    global speech_in_progress
    global blocks
    global q
    data = q.get()
    if data is not None:
        # print(f"{data}\n")
        block = np.asarray([s[0] for s in data], dtype='f')
        volume = sum(abs(s) for s in block)
        # print(f"{volume}\n")
        possible_speech = volume > ambience_threshold
        if possible_speech:
            # Run the tiny model on the segment of speech in progress
            if not speech_in_progress:
                speech_in_progress = True
                active_speech_segment.start_block = bindex
            update_speech_eval(block)
        else:
            speech_ended = speech_in_progress and ((bindex - active_speech_segment.start_block) * block_time) > silence_time
            if speech_ended:
                active_speech_segment.end_block = bindex
                speech_segments.append(SpeechSegment(active_speech_segment.start_block, bindex))
                speech_in_progress = False
                update_speech_eval(block)
        blocks.append(data)
        print(flit.voice_command(str(in_progress_speech_detected))) # "Example string"

instream = sd.InputStream(samplerate=fs, blocksize=block_size, device=sd.default.device, channels=1, dtype='float32', latency='low', extra_settings=None, callback=evaluate_speed, finished_callback=None, clip_off=None, dither_off=None, never_drop_input=None, prime_output_buffers_using_stream_callback=None)
with instream:
    print("Speech recognition systems are live.\n")
    while True:
        process_recording()