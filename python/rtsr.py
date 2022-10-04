import flit
from audioop import maxpp
import whisper
import sounddevice as sd
import types
import numpy as np
import queue
import socket
import threading
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
silence_time = 0.4
phrase_silence_time = 0.5
active_speech_segment = SpeechSegment(0, 0)
speech_in_progress = False

tiny_model = whisper.load_model("tiny")
# medium_model = whisper.load_model("medium")

downsample = 1
mapping = [0]
block_time = .3
block_size = int(fs*block_time)
q = queue.Queue(maxsize=10)
# block_start_offset = 0
# max_blocks_remembered = 
blocks = []
phrase_segments = []
# speech_segments = []
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


# def hd_speech_eval(speech, block):
#     global blocks
#     global medium_model
#     block_group = np.asarray(blocks[speech.start_block:speech.end_block], dtype='f')
#     np.append(block, block_group)
#     block_segment = block_group.flatten()
#     audio = whisper.pad_or_trim(block_segment)
#     mel = whisper.log_mel_spectrogram(audio).to(medium_model.device)
#     options = whisper.DecodingOptions(language='English')
#     result = whisper.decode(medium_model, mel, options)
#     print(f"Speech segment parsed {result.text}")

def process_recording(data):
    pass
    # global speech_in_progress
    # global blocks
    # # print(f"{data}\n")
    # block = np.asarray([s[0] for s in data], dtype='f')
    # volume = sum(abs(s) for s in block)
    # # print(f"{volume}\n")
    # possible_speech = volume > ambience_threshold
    # if possible_speech:
    #     # Run the tiny model on the segment of speech in progress
    #     if not speech_in_progress:
    #         print(f"Speech started at {active_speech_segment.start_block}")
    #         speech_in_progress = True
    #         active_speech_segment.start_block = max(0, bindex-1)
    #     update_speech_eval(block)
    # else:
    #     speech_ended = speech_in_progress and ((bindex - active_speech_segment.start_block) * block_time) > silence_time
    #     if speech_ended:
    #         active_speech_segment.end_block = bindex
    #         ss = SpeechSegment(active_speech_segment.start_block, bindex)
    #         # speech_segments.append(ss)
    #         # hd_speech_eval(ss, block)
    #         speech_in_progress = False
    #         update_speech_eval(block)
    # blocks.append(data)
    # print(flit.voice_command(str(in_progress_speech_detected))) # "Example string"

host = "localhost"
port = 9999

def gen_img(req):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    remote_ip = socket.gethostbyname(host)
    s.connect((remote_ip, port))
    print(f"Stable diffusion request is {req}")
    s.sendall(str.encode(req))
    data = s.recv(1024)
    print('Received', data.decode())
    global q
    q.put(data.decode())

# def get_clipseg(req):
#     global q
#     s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     remote_ip = socket.gethostbyname(host)
#     s.connect((remote_ip, 9998))
#     s.sendall()
#     data = q.get()

instream = sd.InputStream(samplerate=fs, blocksize=block_size, device=sd.default.device, channels=1, dtype='float32', latency='low', extra_settings=None, callback=evaluate_speed, finished_callback=None, clip_off=None, dither_off=None, never_drop_input=None, prime_output_buffers_using_stream_callback=None)
with instream:
    print("Speech recognition systems are live.\n")
    while True:
        req = flit.query_stable_diffusion_requests()
        if req != 0:
            th = threading.Thread(target=gen_img, args=(req,))
            th.start()
        data = q.get()
        if data is not None:
            print(type(data))
            if type(data) is np.ndarray:
                process_recording(data)
            if type(data) is str:
                print("Load target\n")
                flit.load_generated_image(data)
