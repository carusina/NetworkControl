using System;

namespace Gui.ViewModels
{
    // 서버 좌표(월드) -> 캔버스 픽셀로 매핑하는 고정 배율 뷰.
    // 카메라가 엔티티를 따라가지 않고, 원점이 캔버스 중앙에 고정된 전체 맵을 보여준다.
    // 월드 좌표가 시야(WorldExtent)를 벗어나면 캔버스 가장자리에 clamp해서 항상 화면 안에 보이게 함.
    public class EntityViewModel : ViewModelBase
    {
        public const double CanvasSize = 640.0;
        public const double WorldExtent = 300.0; // 원점 기준 -300..+300 이 캔버스에 다 들어감
        private const double MarkerRadius = 8.0;

        private uint entityId_;
        private float positionX_;
        private float positionY_;
        private float heading_;
        private bool isMine_;

        public uint EntityId
        {
            get => entityId_;
            set => SetProperty(ref entityId_, value);
        }

        public bool IsMine
        {
            get => isMine_;
            set => SetProperty(ref isMine_, value);
        }

        public void UpdateState(float positionX, float positionY, float heading)
        {
            if (SetProperty(ref positionX_, positionX, nameof(PositionX))) { RaisePropertyChanged(nameof(CanvasLeft)); }
            if (SetProperty(ref positionY_, positionY, nameof(PositionY))) { RaisePropertyChanged(nameof(CanvasTop)); }
            if (SetProperty(ref heading_, heading, nameof(Heading))) { RaisePropertyChanged(nameof(HeadingDegrees)); }
        }

        public float PositionX => positionX_;
        public float PositionY => positionY_;
        public float Heading => heading_;

        // WPF의 회전 각도는 시계 방향(+)이고, 물리 모델의 Heading은 반시계 방향 라디안이라 부호를 뒤집음
        public double HeadingDegrees => -heading_ * 180.0 / Math.PI;

        public double CanvasLeft => ToCanvas(positionX_) - MarkerRadius;
        public double CanvasTop => ToCanvas(-positionY_) - MarkerRadius; // 화면 Y축은 아래로 증가하므로 반전

        private static double ToCanvas(double worldCoordinate)
        {
            double normalized = (worldCoordinate + WorldExtent) / (WorldExtent * 2.0) * CanvasSize;
            return Math.Max(0.0, Math.Min(CanvasSize, normalized));
        }
    }
}
