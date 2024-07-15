#include "internal_sys_mock.hpp"

#include <linux/ipmi.h>
#include <sys/ioctl.h>

#include <ipmiblob/ipmi_errors.hpp>
#include <ipmiblob/ipmi_handler.hpp>

#include <chrono>
#include <cstring>
#include <thread>

namespace ipmiblob
{

using std::chrono::milliseconds;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Return;

class IpmiHandlerTest : public ::testing::Test
{
  protected:
    IpmiHandlerTest() : sysMock(std::make_unique<internal::InternalSysMock>())
    {}

    void ExpectOpenError(IpmiHandler& ipmi, std::string_view msg)
    {
        EXPECT_THROW(
            {
                try
                {
                    ipmi.open();
                }
                catch (const IpmiException& e)
                {
                    // and this tests that it has the correct message
                    EXPECT_STREQ(msg.data(), e.what());
                    throw;
                }
            },
            IpmiException);
    }

    void ExpectSendPacketError(IpmiHandler& ipmi, std::string_view msg)
    {
        EXPECT_THROW(
            {
                try
                {
                    ipmi.sendPacket(0, 0, data);
                }
                catch (const IpmiException& e)
                {
                    // and this tests that it has the correct message
                    EXPECT_STREQ(msg.data(), e.what());
                    throw;
                }
            },
            IpmiException);
    }

    std::unique_ptr<internal::InternalSysMock> sysMock;
    std::vector<std::uint8_t> data;
    int fd = 1;
    int badFd = -1;
};

TEST_F(IpmiHandlerTest, OpenAllFails)
{
    /* Open against all device files fail. */
    EXPECT_CALL(*sysMock, open(_, _)).WillRepeatedly(Return(badFd));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectOpenError(ipmi, "Unable to open any ipmi devices");
}

TEST_F(IpmiHandlerTest, OpenSucess)
{
    EXPECT_CALL(*sysMock, open(_, _))
        .WillOnce(Return(badFd))
        .WillOnce(Return(badFd))
        .WillOnce(Return(1));

    IpmiHandler ipmi(std::move(sysMock));
    EXPECT_NO_THROW(ipmi.open());
}

TEST_F(IpmiHandlerTest, SendPacketOpenAllFails)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillRepeatedly(Return(badFd));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectOpenError(ipmi, "Unable to open any ipmi devices");
    ExpectOpenError(ipmi, "Unable to open any ipmi devices");
}

ACTION_TEMPLATE(SetArgNPointeeTo, HAS_1_TEMPLATE_PARAMS(unsigned, uIndex),
                AND_2_VALUE_PARAMS(pData, uiDataSize))
{
    ipmi_recv* reply = reinterpret_cast<ipmi_recv*>(std::get<uIndex>(args));
    std::memcpy(reply->msg.data, pData, uiDataSize);
    reply->msg.data_len = uiDataSize;
}

TEST_F(IpmiHandlerTest, SendPacketRequestFailed)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(badFd));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectSendPacketError(ipmi, "Unable to send IPMI request.");
}

TEST_F(IpmiHandlerTest, SendPacketPollingError)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(0));
    EXPECT_CALL(*sysMock, poll(_, 1, _)).WillOnce(Return(badFd));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectSendPacketError(ipmi, "Polling Error occurred.");
}

TEST_F(IpmiHandlerTest, SendPacketPollTimeout)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(0));
    EXPECT_CALL(*sysMock, poll(_, 1, _)).WillOnce(Return(0));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectSendPacketError(ipmi, "Timeout waiting for reply.");
}

TEST_F(IpmiHandlerTest, SendPacketBadIpmiReply)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(0));
    EXPECT_CALL(*sysMock, poll(_, 1, _)).WillOnce(Return(1));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, _))
        .WillOnce(Return(badFd));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectSendPacketError(ipmi, "Unable to read reply.");
}

TEST_F(IpmiHandlerTest, SendPacketNotIpmiOk)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(0));
    EXPECT_CALL(*sysMock, poll(_, 1, _)).WillOnce(Return(1));

    std::vector<std::uint8_t> expectedOutput = {2};

    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, _))
        .WillOnce(DoAll(
            SetArgNPointeeTo<2>(expectedOutput.data(), expectedOutput.size()),
            Return(0)));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectSendPacketError(ipmi, "Received IPMI_CC: 2");
}

TEST_F(IpmiHandlerTest, SendPacketFailedOpenOnce)
{
    EXPECT_CALL(*sysMock, open(_, _))
        .WillOnce(Return(badFd))
        .WillOnce(Return(badFd))
        .WillOnce(Return(badFd))
        .WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(0));
    EXPECT_CALL(*sysMock, poll(_, 1, _)).WillOnce(Return(1));

    std::vector<std::uint8_t> expectedOutput = {0, 1, 2, 3};

    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, _))
        .WillOnce(DoAll(
            SetArgNPointeeTo<2>(expectedOutput.data(), expectedOutput.size()),
            Return(0)));

    IpmiHandler ipmi(std::move(sysMock));

    ExpectSendPacketError(ipmi, "Unable to open any ipmi devices");
    EXPECT_THAT(ipmi.sendPacket(0, 0, data), ElementsAre(1, 2, 3));
}

TEST_F(IpmiHandlerTest, SendPacketSucess)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(0));
    EXPECT_CALL(*sysMock, poll(_, 1, _)).WillOnce(Return(1));

    std::vector<std::uint8_t> expectedOutput = {0, 1, 2, 3};

    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_RECEIVE_MSG_TRUNC, _))
        .WillOnce(DoAll(
            SetArgNPointeeTo<2>(expectedOutput.data(), expectedOutput.size()),
            Return(0)));

    IpmiHandler ipmi(std::move(sysMock));
    EXPECT_THAT(ipmi.sendPacket(0, 0, data), ElementsAre(1, 2, 3));
}

ACTION_TEMPLATE(SetOpenDelays, HAS_1_TEMPLATE_PARAMS(unsigned, delay),
                AND_0_VALUE_PARAMS())
{
    std::this_thread::sleep_for(milliseconds(delay));
}

// Tried to call open() in different thread and making sure that both thread
// tried it. Instead of only calling open() once on failures.
TEST_F(IpmiHandlerTest, SendPacketTriedOpenInParallel)
{
    EXPECT_CALL(*sysMock, open(_, _))
        .WillOnce(DoAll(SetOpenDelays<200>(), Return(badFd)))
        .WillOnce(DoAll(SetOpenDelays<200>(), Return(badFd)))
        .WillOnce(DoAll(SetOpenDelays<200>(), Return(badFd)))
        // Make sure the second thread will wait until call_once finished and
        // then retry.
        .WillOnce(DoAll(SetOpenDelays<0>(), Return(badFd)))
        .WillOnce(DoAll(SetOpenDelays<0>(), Return(badFd)))
        .WillOnce(DoAll(SetOpenDelays<0>(), Return(badFd)));

    IpmiHandler ipmi(std::move(sysMock));
    auto testOpenParallel = [this, &ipmi]() {
        ExpectOpenError(ipmi, "Unable to open any ipmi devices");
    };

    std::thread t1(testOpenParallel);
    std::thread t2(testOpenParallel);
    t1.join();
    t2.join();
}

} // namespace ipmiblob
