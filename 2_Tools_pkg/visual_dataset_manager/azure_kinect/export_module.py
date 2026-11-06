import os
import cv2

# 类别编号映射，可按需扩充
CLASS_MAP = {
    "person": 0,
    "box": 1,
    "cup": 2
}
REVERSE_CLASS = {v: k for k, v in CLASS_MAP.items()}

# 导出YOLO TXT格式
def export_yolo(img_path, boxes, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    h, w, _ = cv2.imread(img_path).shape
    img_name = os.path.basename(img_path).rsplit(".", 1)[0]
    txt_save_path = os.path.join(output_dir, f"{img_name}.txt")
    content_lines = []
    for box in boxes:
        x1, y1, x2, y2 = box["x1"], box["y1"], box["x2"], box["y2"]
        cls_name = box["cls"]
        cls_id = CLASS_MAP.get(cls_name, 0)
        # YOLO标准归一化坐标计算
        center_x = ((x1 + x2) / 2) / w
        center_y = ((y1 + y2) / 2) / h
        box_w = (x2 - x1) / w
        box_h = (y2 - y1) / h
        content_lines.append(f"{cls_id} {center_x:.6f} {center_y:.6f} {box_w:.6f} {box_h:.6f}")
    with open(txt_save_path, "w", encoding="utf-8") as f:
        f.write("\n".join(content_lines))

# 导出VOC XML格式
def export_voc_xml(img_path, boxes, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    h, w, channel = cv2.imread(img_path).shape
    full_filename = os.path.basename(img_path)
    img_name = full_filename.rsplit(".", 1)[0]
    xml_save_path = os.path.join(output_dir, f"{img_name}.xml")
    object_xml = ""
    for box in boxes:
        x1, y1, x2, y2 = box["x1"], box["y1"], box["x2"], box["y2"]
        cls = box["cls"]
        object_xml += f"""
  <object>
    <name>{cls}</name>
    <bndbox>
      <xmin>{x1}</xmin>
      <ymin>{y1}</ymin>
      <xmax>{x2}</xmax>
      <ymax>{y2}</ymax>
    </bndbox>
  </object>
"""
    xml_text = f"""<annotation>
  <folder>dataset</folder>
  <filename>{full_filename}</filename>
  <size>
    <width>{w}</width>
    <height>{h}</height>
    <depth>{channel}</depth>
  </size>
{object_xml}
</annotation>
"""
    with open(xml_save_path, "w", encoding="utf-8") as f:
        f.write(xml_text)