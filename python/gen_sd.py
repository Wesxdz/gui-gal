# import subprocess

# subprocess.run('. /home/aeri/miniconda3/etc/profile.d/conda.sh && conda activate ldm', shell=True, executable='/bin/bash')

import argparse

import torch
from torch.cuda.amp import autocast
from diffusers import StableDiffusionPipeline, DDIMScheduler
import random
# def test_function():
#     print("Hello!");
parser = argparse.ArgumentParser()
parser.add_argument(
    "--prompt",
    type=str,
    nargs="?",
    default="hello, gui",
    help="the prompt to render"
)
parser.add_argument(
    "--seed",
    type=int,
    nargs="?",
    default=0,
    help="the seed to use"
)
opt = parser.parse_args()

model_id = "home/aeri/projects/gui-gal/deps/stable-diffusion/models/ldm/stable-diffusion-v1/"
device = "cuda"

pipe = StableDiffusionPipeline.from_pretrained(model_id, use_auth_token=False)
pipe = pipe.to(device)
def dummy(images, **kwargs): return images, False
pipe.safety_checker = dummy

seed = opt.seed # random.randint(-100000000, 0)
interpolate_distance = 3
generator = torch.Generator("cuda").manual_seed(seed)
image = pipe(opt.prompt, guidance_scale=7.5, generator=generator, num_inference_steps=50, width=512, height=512)["sample"][0]  
    
image.save("example_" + str(seed) + ".png")