import os
import random
import shutil

# This script create the structure for the dataset and splits 
# the dataset into 3 subcategories: train, val, test
# each subcategorie has 2 directories: images and labels
# For now only jpg images are supported. 
# Images without labels need to be deletet manually, will be logged in terminal for 
# easy search & delete.

# Paths
root = r"C:\Users\jendr\Desktop\\"  # Replace with your root path
input_dir = root + r"\segmentation_data"  # Replace with your directory containing images and labels
output_dir = root + r"\segmentation_data\dataset"  # Output directory for train/val/test splits
os.makedirs(f"{output_dir}/train/images", exist_ok=True)
os.makedirs(f"{output_dir}/train/labels", exist_ok=True)
os.makedirs(f"{output_dir}/val/images", exist_ok=True)
os.makedirs(f"{output_dir}/val/labels", exist_ok=True)
os.makedirs(f"{output_dir}/test/images", exist_ok=True)
os.makedirs(f"{output_dir}/test/labels", exist_ok=True)

# Parameters -- Best practice is to use 70% train, 20% val, 10% test
train_ratio = 0.7
val_ratio = 0.2
test_ratio = 0.1

# Get all image files
images = [f for f in os.listdir(input_dir) if f.endswith(('.jpg'))]
random.shuffle(images)

# Split dataset
train_split = int(train_ratio * len(images))
val_split = int((train_ratio + val_ratio) * len(images))

train_images = images[:train_split] # from begin to train_split
val_images = images[train_split:val_split] # from train_split to val_split
test_images = images[val_split:] # from val_split to end

# Function to move files
def move_files(split, split_images):
    copied_images_counter = 0
    non_copied_images_counter = 0


    for img in split_images:
        label = os.path.splitext(img)[0] + ".txt"  # Corresponding label file
        img_src = os.path.join(input_dir, img)
        label_src = os.path.join(input_dir, label)

        # Move image

        # Check if label exists
        if os.path.exists(label_src):
            # Move label and image to respective directories
            shutil.copy(img_src, f"{output_dir}/{split}/images/{img}")
            shutil.copy(label_src, f"{output_dir}/{split}/labels/{label}")
            copied_images_counter += 1

        else:
            print(f"Warning: Label file not found for image {img}")
            non_copied_images_counter += 1

    print(f"Moved {copied_images_counter} images and labels to {split} set. \n")
    print(f"Skipped {non_copied_images_counter} images due to missing labels. \n")





# Move files to respective directories
move_files("train", train_images)
move_files("val", val_images)
move_files("test", test_images)

print("Dataset split completed!")