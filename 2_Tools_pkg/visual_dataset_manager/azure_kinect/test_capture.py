from capture_module import get_capture, save_rgb_image
import cv2

cap = get_capture()
while True:
    frame = cap.get_frame()
    cv2.imshow("预览窗口", frame)
    key = cv2.waitKey(30)
    if key == ord("s"):
        # 保存至任务test、场景scene01
        save_rgb_image(frame, "test", "scene01")
        print("图片已保存")
    if key == ord("q"):
        break
cap.close()
cv2.destroyAllWindows()