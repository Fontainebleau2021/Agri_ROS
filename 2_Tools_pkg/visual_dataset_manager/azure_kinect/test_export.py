from label_module import ImageLabel
from export_module import export_yolo, export_voc_xml

img_path = "./dataset/test/scene01/20260704_160806.jpg"
label = ImageLabel(img_path)
label.load()

# 导出YOLO格式，保存到yolo_out文件夹
export_yolo(img_path, label.boxes, "./yolo_out")
# 导出VOC格式，保存到voc_out文件夹
export_voc_xml(img_path, label.boxes, "./voc_out")
print("数据集导出完成")