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

ACTION_TEMPLATE(SetArgNPointeeTo, HAS_1_TEMPLATE_PARAMS(unsigned, uIndex),
                AND_2_VALUE_PARAMS(pData, uiDataSize))
{
    ipmi_recv* reply = reinterpret_cast<ipmi_recv*>(std::get<uIndex>(args));
    std::memcpy(reply->msg.data, pData, uiDataSize);
    reply->msg.data_len = uiDataSize;
}

ACTION_TEMPLATE(SetOpenDelays, HAS_1_TEMPLATE_PARAMS(unsigned, delay),
                AND_0_VALUE_PARAMS())
{
    std::this_thread::sleep_for(milliseconds(delay));
}

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
    // Make sure that we don't throw any exception when getting bad file
    // descriptor Return valid file descriptor at the last ipmi device for a
    // succeful open().
    EXPECT_CALL(*sysMock, open(_, _))
        .WillOnce(Return(badFd))
        .WillOnce(Return(badFd))
        .WillOnce(Return(fd));

    IpmiHandler ipmi(std::move(sysMock));
    EXPECT_NO_THROW(ipmi.open());
}

TEST_F(IpmiHandlerTest, SendPacketOpenAllFails)
{
    EXPECT_CALL(*sysMock, open(_, _)).WillRepeatedly(Return(badFd));

    IpmiHandler ipmi(std::move(sysMock));
    ExpectSendPacketError(ipmi, "Unable to open any ipmi devices");
    ExpectSendPacketError(ipmi, "Unable to open any ipmi devices");
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

// Tried to call open() in different thread and making sure that both thread
// tried it and there aree no data race. Expect the first thread to fail to
// open() and second one pass open(), but failed in the IPMICTL_SEND_COMMAND.
TEST_F(IpmiHandlerTest, SendPacketTriedOpenInParallel)
{
    EXPECT_CALL(*sysMock, open(_, _))
        // The badFd is expected to be used in testOpenParallel0 and need enough
        // delay to make sure that testOpenParallel1 starts before
        // testOpenParallel0 finishes.
        .WillOnce(DoAll(SetOpenDelays<10>(), Return(badFd)))
        .WillOnce(DoAll(SetOpenDelays<10>(), Return(badFd)))
        .WillOnce(DoAll(SetOpenDelays<10>(), Return(badFd)))
        .WillOnce(Return(fd));
    EXPECT_CALL(*sysMock, ioctl(fd, IPMICTL_SEND_COMMAND, _))
        .WillOnce(Return(-1));

    IpmiHandler ipmi(std::move(sysMock));
    auto testOpenParallel0 = [this, &ipmi]() {
        ExpectSendPacketError(ipmi, "Unable to open any ipmi devices");
    };

    auto testOpenParallel1 = [this, &ipmi]() {
        // Make sure this start after testOpenParallel0 get to the open()
        std::this_thread::sleep_for(milliseconds(10));
        EXPECT_THROW(
            {
                std::vector<std::uint8_t> data1;
                try
                {
                    ipmi.sendPacket(0, 0, data1);
                }
                catch (const IpmiException& e)
                {
                    EXPECT_STREQ("Unable to send IPMI request.", e.what());
                    throw;
                }
            },
            IpmiException);
    };

    std::thread t1(testOpenParallel0);
    std::thread t2(testOpenParallel1);
    t1.join();
    t2.join();
}

} // namespace ipmiblob
