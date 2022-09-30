import torch
import requests
from PIL import Image
from torchvision import transforms
from matplotlib import pyplot as plt
import torchvision.transforms as T

model = CLIPDensePredT(version='ViT-B/16', reduce_dim=64)
model.eval()
model.load_state_dict(torch.load('weights/rd64-uni.pth', map_location=torch.device('cpu')), strict=False)

input_image = Image.open('example_image.jpg')

# or load from URL...
# image_url = 'https://farm5.staticflickr.com/4141/4856248695_03475782dc_z.jpg'
# input_image = Image.open(requests.get(image_url, stream=True).raw)

def get_mask(img_path, prompt):
    input_image = Image.open(img_path)
    transform = transforms.Compose([
        transforms.ToTensor(),
        transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]),
        transforms.Resize((352, 352)),
    ])
    img = transform(input_image).unsqueeze(0)

    prompts = [prompt]

    # predict
    with torch.no_grad():
        preds = model(img.repeat(4,1,1,1), prompts)[0]

    mask = T.ToPILImage(torch.sigmoid(preds[0][0]))
    mask.save()