namespace Namorix.Weave.BorderRouter.Frame;

public enum BrCommand : byte
{
    Data = 0x01,
    Ack = 0x02,
    Nack = 0x03,
    Reset = 0x10,
    Factory = 0x11,
    State = 0x12,
    IpAddr = 0x13,
    DatasetActive = 0x14,
    MacAddress = 0x16,
    BrHealth = 0x17,
    SetPanId = 0x20,
    SetChannel = 0x21,
    SetNetworkName = 0x22,
    SetExtendedPanId = 0x23,
    SetNetworkKey = 0x24,
    RouterTable = 0x30,
    ChildTable = 0x31,
    JoinerTable = 0x32,
    ThreadStart = 0x40,
    ThreadStop = 0x41,
    ThreadVersion = 0x42,
    CommissionerJoiner = 0x43,
    SrpRegister = 0x44,
    Notify = 0x45,
}
