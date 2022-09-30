from transformers import pipeline
import numpy as np

classifier = pipeline('zero-shot-classification')
shot = classifier(
    "Highlight the flowers",
    candidate_labels=["Create image from text prompt", "Indicate masked segment of image"]
)
print(shot)
print(shot['labels'][np.argmax(shot['scores'])])