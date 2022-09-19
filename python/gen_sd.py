import torch
from torch.cuda.amp import autocast
from diffusers import StableDiffusionPipeline, DDIMScheduler
import random

model_id = "CompVis/stable-diffusion-v1-4"
device = "cuda"

pipe = StableDiffusionPipeline.from_pretrained(model_id, use_auth_token=True)
pipe = pipe.to(device)
def dummy(images, **kwargs): return images, False
pipe.safety_checker = dummy

seed = random.randint(-100000000, 0)
interpolate_distance = 3
generator = torch.Generator("cuda").manual_seed(seed)
prompt = "Hello, Gui."
image = pipe(prompt, guidance_scale=7.5, generator=generator, num_inference_steps=50, width=512, height=512)["sample"][0]  
    
image.save("example_" + str(seed) + ".png")