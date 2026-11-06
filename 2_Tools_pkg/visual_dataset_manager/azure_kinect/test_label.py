from label_module import ImageLabel

# 替换成你dataset里任意一张图片的完整路径
img_file = "./dataset/test/scene01/20260704_160806.jpg"
label = ImageLabel(img_file)

# 添加一个标注框，类别person
label.add_box(100, 100, 400, 500, "person")
# 保存标注，同目录自动生成xxx.json
label.save()

# 重新加载标注，打印查看
label.load()
print("当前所有标注：", label.boxes)