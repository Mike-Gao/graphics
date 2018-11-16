# this is a gradient based noise attack, works on mobilenet
import numpy as np
from keras.preprocessing import image
from keras.applications import mobilenet
from keras.applications.mobilenet import preprocess_input
from keras.models import load_model
from keras import backend as K
from PIL import Image

# load the pretrained model
model = load_model('model.h5')

# get first and last layer of the neural net for reference
model_input_layer = model.layers[0].input
model_output_layer = model.layers[-1].output

# Choose an ImageNet object to fake, list available here: https://gist.github.com/ageitgey/4e1342c10a71981d0b491e1b8227328b
object_type_to_fake = 31

# Load the image
img = image.load_img("trixi.png", target_size=(224, 224))
original_image = image.img_to_array(img)

# Scale the image so the pixels are between -1 and 1, which is what exactly the model expects
original_image /= 255.
original_image -= 0.5
original_image *= 2.

# Add another dimension for "batch size"
original_image = np.expand_dims(original_image, axis=0)

# define the "maximum change" we want the image to have. 
# We'll make sure the image does not go past this value since we don't want too much distortion
max_change_above = original_image + 0.1
max_change_below = original_image - 0.1

# create a copy of the image for pertubation
hacked_image = np.copy(original_image)

# how much do we want to update the image
learning_rate = 2000000000000000

# Define the cost function.
cost_function = model_output_layer[0, object_type_to_fake]

# Calculate the gradient based on the input image and the input class
gradient_function = K.gradients(cost_function, model_input_layer)[0]

# Create a Keras function for cost and gradient calculation
grab_cost_and_gradients_from_model = K.function([model_input_layer, K.learning_phase()], [cost_function, gradient_function])

cost = 0.0

# Keep adjusting the image until the confidence is above .98
while cost < 0.98:
    # Check how close the image is to our target class
    # Retrieve radients we can use it to further our goal
    cost, gradients = grab_cost_and_gradients_from_model([hacked_image, 0])
    print(cost_function)
    print(gradients) # debug
    hacked_image += gradients * learning_rate

    # Ensure image does not change too much
    hacked_image = np.clip(hacked_image, max_change_below, max_change_above)
    hacked_image = np.clip(hacked_image, -1.0, 1.0)

    print("Model's predicted likelihood that the image is a tree_frog: {:.8}%".format(cost * 100))

# Deprocess the image
img = hacked_image[0]
img /= 2.
img += 0.5
img *= 255.

# Save the image
im = Image.fromarray(img.astype(np.uint8))
im.save("hacked-image.png")
