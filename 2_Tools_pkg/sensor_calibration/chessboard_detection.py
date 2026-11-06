print("Starting script")
import cv2
from cv_bridge import CvBridge
from rosbag2_py import SequentialReader, StorageOptions

# Path to the ROS2 bag file (adjust if needed)
bag_path = '/mnt/d/smart-agriculture-tool/课程作业/课程作业/project/data/标定数据'
topic = '/s_gmsl_6/image_raw/compressed'

print("Opening bag...")
# Initialize the reader
reader = SequentialReader()
reader.open(StorageOptions(uri=bag_path, storage_id='sqlite3'))

print("Bag opened.")
# Initialize CvBridge
bridge = CvBridge()

# Flag to track if chessboard is found in any image
found = False
count = 0

# Iterate through the bag messages
while reader.has_next():
    data = reader.read_next()
    print(f"Topic: {data.topic_name}")
    if data.topic_name == topic:
        msg = data.message
        count += 1
        print(f"Processing image {count}")
        try:
            # Convert compressed image to OpenCV format
            cv_image = bridge.compressed_imgmsg_to_cv2(msg, desired_encoding='bgr8')
            print(f"Image shape: {cv_image.shape}")
            # Check for 7x5 chessboard pattern
            ret, corners = cv2.findChessboardCorners(cv_image, (7, 5))
            print(f"Chessboard found: {ret}")
            if ret:
                found = True
                break  # Stop at the first found chessboard
        except Exception as e:
            print(f"Error processing image: {e}")

# Print the result
if found:
    print("Chessboard found in at least one image")
else:
    print("Chessboard not found in any image")
print("Script end")