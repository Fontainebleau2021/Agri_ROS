import json
import os

class ImageLabel:
    def __init__(self, img_path):
        self.img_path = img_path
        # 标注文件和图片同目录，同名json
        self.label_path = img_path.rsplit(".", 1)[0] + ".json"
        self.boxes = []  # 存储所有标注框，格式：{"x1","y1","x2","y2","cls"}

    # 添加标注框
    def add_box(self, x1, y1, x2, y2, cls_name):
        self.boxes.append({
            "x1": x1, "y1": y1,
            "x2": x2, "y2": y2,
            "cls": cls_name
        })

    # 删除指定下标标注框
    def delete_box(self, index):
        if 0 <= index < len(self.boxes):
            del self.boxes[index]

    # 清空整张图所有标注
    def clear_all(self):
        self.boxes.clear()

    # 将标注保存到本地json文件
    def save(self):
        with open(self.label_path, "w", encoding="utf-8") as f:
            json.dump(self.boxes, f, indent=2, ensure_ascii=False)

    # 读取本地已存在的标注
    def load(self):
        if os.path.exists(self.label_path):
            with open(self.label_path, "r", encoding="utf-8") as f:
                self.boxes = json.load(f)