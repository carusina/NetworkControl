using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Threading;
using ClientCoreManaged;

namespace Gui.ViewModels
{
    public class MainViewModel : ViewModelBase, IDisposable
    {
        // GetEntities()/GetMetrics()를 폴링하는 주기 - 서버 기본 스트리밍 주기(30Hz)에 맞춤
        private static readonly TimeSpan PollInterval = TimeSpan.FromMilliseconds(1000.0 / 30.0);

        private readonly ManagedGameClient client_ = new ManagedGameClient();
        private readonly DispatcherTimer timer_;
        private readonly Dictionary<uint, EntityViewModel> entityLookup_ = new Dictionary<uint, EntityViewModel>();

        private string host_ = "127.0.0.1";
        private string tcpPortText_ = "5000";
        private string udpPortText_ = "6000";
        private string serverUdpPortText_ = "6500";
        private bool isConnected_;
        private string statusMessage_ = "연결되지 않음";
        private double throttle_;
        private double yaw_;
        private string myEntityIdText_ = "-";
        private string myPositionText_ = "-";
        private ManagedMetricsSnapshot metrics_;

        // 서버가 현재 재생 상태/전송 주기를 알려주는 메시지가 없어서, 버튼을 누른 결과를
        // 로컬에서 낙관적으로(성공 시 즉시) 추적함 - Play/Pause/Stop만 상태를 바꾸고,
        // Reset은 서버 쪽 SessionState를 안 건드리므로 여기서도 상태를 안 바꿈.
        private enum PlayState { Stopped, Playing, Paused }
        private PlayState playState_ = PlayState.Stopped;
        private bool isRate60_;

        public MainViewModel()
        {
            timer_ = new DispatcherTimer { Interval = PollInterval };
            timer_.Tick += (_, __) => Poll();

            ConnectCommand = new RelayCommand(_ => Connect(), _ => !IsConnected);
            PlayCommand = new RelayCommand(_ => { if (client_.Play()) SetPlayState(PlayState.Playing); }, _ => IsConnected);
            PauseCommand = new RelayCommand(_ => { if (client_.Pause()) SetPlayState(PlayState.Paused); }, _ => IsConnected);
            StopCommand = new RelayCommand(_ => Stop(), _ => IsConnected);
            ResetCommand = new RelayCommand(_ => client_.Reset(), _ => IsConnected);
            SetRate30Command = new RelayCommand(_ => { if (client_.SetRate30()) SetDataRate60(false); }, _ => IsConnected);
            SetRate60Command = new RelayCommand(_ => { if (client_.SetRate60()) SetDataRate60(true); }, _ => IsConnected);
        }

        public ObservableCollection<EntityViewModel> Entities { get; } = new ObservableCollection<EntityViewModel>();

        public string Host
        {
            get => host_;
            set => SetProperty(ref host_, value);
        }

        public string TcpPortText
        {
            get => tcpPortText_;
            set => SetProperty(ref tcpPortText_, value);
        }

        public string UdpPortText
        {
            get => udpPortText_;
            set => SetProperty(ref udpPortText_, value);
        }

        public string ServerUdpPortText
        {
            get => serverUdpPortText_;
            set => SetProperty(ref serverUdpPortText_, value);
        }

        public bool IsConnected
        {
            get => isConnected_;
            private set => SetProperty(ref isConnected_, value);
        }

        public string StatusMessage
        {
            get => statusMessage_;
            private set => SetProperty(ref statusMessage_, value);
        }

        public string MyEntityIdText
        {
            get => myEntityIdText_;
            private set => SetProperty(ref myEntityIdText_, value);
        }

        public string MyPositionText
        {
            get => myPositionText_;
            private set => SetProperty(ref myPositionText_, value);
        }

        public bool IsPlayActive => playState_ == PlayState.Playing;
        public bool IsPauseActive => playState_ == PlayState.Paused;
        public bool IsStopActive => playState_ == PlayState.Stopped;

        public string PlayStateText
        {
            get
            {
                switch (playState_)
                {
                    case PlayState.Playing: return "Playing";
                    case PlayState.Paused: return "Paused";
                    default: return "Stopped";
                }
            }
        }

        public bool IsRate30Active => !isRate60_;
        public bool IsRate60Active => isRate60_;
        public string DataRateText => isRate60_ ? "60Hz" : "30Hz";

        public ManagedMetricsSnapshot Metrics
        {
            get => metrics_;
            private set => SetProperty(ref metrics_, value);
        }

        // 슬라이더를 드래그할 때마다 값이 바뀌고, 그때마다 서버로 조종 입력을 보냄
        public double Throttle
        {
            get => throttle_;
            set
            {
                if (SetProperty(ref throttle_, value))
                {
                    SendControlInput();
                }
            }
        }

        public double Yaw
        {
            get => yaw_;
            set
            {
                if (SetProperty(ref yaw_, value))
                {
                    SendControlInput();
                }
            }
        }

        public RelayCommand ConnectCommand { get; }
        public RelayCommand PlayCommand { get; }
        public RelayCommand PauseCommand { get; }
        public RelayCommand StopCommand { get; }
        public RelayCommand ResetCommand { get; }
        public RelayCommand SetRate30Command { get; }
        public RelayCommand SetRate60Command { get; }

        private void SetPlayState(PlayState state)
        {
            playState_ = state;
            RaisePropertyChanged(nameof(IsPlayActive));
            RaisePropertyChanged(nameof(IsPauseActive));
            RaisePropertyChanged(nameof(IsStopActive));
            RaisePropertyChanged(nameof(PlayStateText));
        }

        private void SetDataRate60(bool isRate60)
        {
            isRate60_ = isRate60;
            RaisePropertyChanged(nameof(IsRate30Active));
            RaisePropertyChanged(nameof(IsRate60Active));
            RaisePropertyChanged(nameof(DataRateText));
        }

        private void Stop()
        {
            if (!client_.Stop())
            {
                return;
            }

            SetPlayState(PlayState.Stopped);

            // 서버도 Stop 시 조종 입력을 0으로 되돌리므로, 슬라이더도 맞춰서 0으로 되돌림
            Throttle = 0.0;
            Yaw = 0.0;
        }

        private void SendControlInput()
        {
            if (IsConnected)
            {
                client_.SendControlInput((float)Throttle, (float)Yaw);
            }
        }

        private void Connect()
        {
            if (!int.TryParse(TcpPortText, out int tcpPort) ||
                !int.TryParse(UdpPortText, out int udpPort) ||
                !int.TryParse(ServerUdpPortText, out int serverUdpPort))
            {
                StatusMessage = "포트 값이 올바르지 않음";
                return;
            }

            bool connected = client_.Connect(Host, tcpPort, udpPort, serverUdpPort);
            if (!connected)
            {
                StatusMessage = "연결 실패";
                return;
            }

            IsConnected = true;
            StatusMessage = "연결됨";
            // 새 세션은 서버 기본값(Stopped, 30Hz)으로 시작하므로 로컬 표시도 맞춰줌
            SetPlayState(PlayState.Stopped);
            SetDataRate60(false);
            timer_.Start();
        }

        private void Poll()
        {
            var seenIds = new HashSet<uint>();
            var myEntityId = client_.GetMyEntityId();

            foreach (var entity in client_.GetEntities())
            {
                seenIds.Add(entity.EntityId);

                if (!entityLookup_.TryGetValue(entity.EntityId, out var vm))
                {
                    vm = new EntityViewModel { EntityId = entity.EntityId };
                    entityLookup_[entity.EntityId] = vm;
                    Entities.Add(vm);
                }

                vm.UpdateState(entity.PositionX, entity.PositionY, entity.Heading);
                vm.IsMine = myEntityId.HasValue && myEntityId.Value == entity.EntityId;
            }

            // 더 이상 서버가 알려주지 않는(Despawn된) 엔티티는 화면에서도 제거
            var staleIds = entityLookup_.Keys.Except(seenIds).ToList();
            foreach (var staleId in staleIds)
            {
                Entities.Remove(entityLookup_[staleId]);
                entityLookup_.Remove(staleId);
            }

            // 카메라 중심/헤딩 = 내 엔티티의 현재 위치/헤딩 (아직 스폰 전이면 월드 원점, 헤딩 0)
            double cameraX = 0.0;
            double cameraY = 0.0;
            double cameraHeading = 0.0;
            EntityViewModel myVm = null;
            if (myEntityId.HasValue && entityLookup_.TryGetValue(myEntityId.Value, out myVm))
            {
                cameraX = myVm.PositionX;
                cameraY = myVm.PositionY;
                cameraHeading = myVm.Heading;
            }
            foreach (var vm in entityLookup_.Values)
            {
                vm.UpdateCanvasPosition(cameraX, cameraY, cameraHeading);
            }

            MyEntityIdText = myEntityId.HasValue ? myEntityId.Value.ToString() : "-";
            MyPositionText = myVm != null ? $"X: {myVm.PositionX:F1}  Y: {myVm.PositionY:F1}" : "-";
            Metrics = client_.GetMetrics();
        }

        public void Dispose()
        {
            timer_.Stop();
            if (IsConnected)
            {
                client_.Disconnect();
            }
            client_.Dispose();
        }
    }
}
