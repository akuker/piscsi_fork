//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/scsi.h"
#include "shared/piscsi_exceptions.h"
#include "controllers/scsi_controller.h"

using namespace scsi_defs;

TEST(ScsiControllerTest, GetInitiatorId)
{
	const int ID = 2;

	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.Process(ID);
	EXPECT_EQ(ID, controller.GetInitiatorId());
	controller.Process(-1);
	EXPECT_EQ(-1, controller.GetInitiatorId());
}

TEST(ScsiControllerTest, Process)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::reserved);
	ON_CALL(*bus, GetRST).WillByDefault(Return(true));
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST);
	EXPECT_CALL(*bus, Reset);
	EXPECT_CALL(controller, Reset);
	EXPECT_EQ(phase_t::reserved, controller.Process(0));

	controller.SetPhase(phase_t::busfree);
	ON_CALL(*bus, GetRST).WillByDefault(Return(false));
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST);
	EXPECT_EQ(phase_t::busfree, controller.Process(0));

	controller.SetPhase(phase_t::reserved);
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST);
	EXPECT_CALL(*bus, Reset);
	EXPECT_CALL(controller, Reset);
	EXPECT_EQ(phase_t::busfree, controller.Process(0));
}

TEST(ScsiControllerTest, BusFree)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::busfree);
	controller.BusFree();
	EXPECT_EQ(phase_t::busfree, controller.GetPhase());

	controller.SetStatus(status::CHECK_CONDITION);
	controller.SetPhase(phase_t::reserved);
	controller.BusFree();
	EXPECT_EQ(phase_t::busfree, controller.GetPhase());
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	controller.ScheduleShutdown(AbstractController::piscsi_shutdown_mode::NONE);
	controller.SetPhase(phase_t::reserved);
	controller.BusFree();

	controller.ScheduleShutdown(AbstractController::piscsi_shutdown_mode::STOP_PI);
	controller.SetPhase(phase_t::reserved);
	controller.BusFree();

	controller.ScheduleShutdown(AbstractController::piscsi_shutdown_mode::RESTART_PI);
	controller.SetPhase(phase_t::reserved);
	controller.BusFree();

	controller.ScheduleShutdown(AbstractController::piscsi_shutdown_mode::STOP_PISCSI);
	controller.SetPhase(phase_t::reserved);
	EXPECT_EXIT(controller.BusFree(), ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(ScsiControllerTest, Selection)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockScsiController>(controller_manager, 0);

	controller->SetPhase(phase_t::selection);
	ON_CALL(*bus, GetSEL).WillByDefault(Return(true));
	ON_CALL(*bus, GetBSY).WillByDefault(Return(true));
	EXPECT_CALL(*bus, GetATN).Times(0);
	controller->Selection();
	EXPECT_EQ(phase_t::selection, controller->GetPhase());

	ON_CALL(*bus, GetSEL).WillByDefault(Return(true));
	ON_CALL(*bus, GetBSY).WillByDefault(Return(false));
	EXPECT_CALL(*bus, GetATN).Times(0);
	EXPECT_CALL(*controller, Status);
	controller->Selection();
	EXPECT_EQ(phase_t::selection, controller->GetPhase());

	ON_CALL(*bus, GetSEL).WillByDefault(Return(false));
	ON_CALL(*bus, GetBSY).WillByDefault(Return(false));
	EXPECT_CALL(*bus, GetATN).Times(0);
	controller->Selection();
	EXPECT_EQ(phase_t::selection, controller->GetPhase());

	ON_CALL(*bus, GetSEL).WillByDefault(Return(false));
	ON_CALL(*bus, GetBSY).WillByDefault(Return(true));
	ON_CALL(*bus, GetATN).WillByDefault(Return(false));
	EXPECT_CALL(*bus, GetATN);
	controller->Selection();
	EXPECT_EQ(phase_t::command, controller->GetPhase());

	controller->SetPhase(phase_t::selection);
	ON_CALL(*bus, GetSEL).WillByDefault(Return(false));
	ON_CALL(*bus, GetBSY).WillByDefault(Return(true));
	ON_CALL(*bus, GetATN).WillByDefault(Return(true));
	EXPECT_CALL(*bus, GetATN);
	controller->Selection();
	EXPECT_EQ(phase_t::msgout, controller->GetPhase());

	controller->SetPhase(phase_t::reserved);
	ON_CALL(*bus, GetDAT).WillByDefault(Return(0));
	controller->Selection();
	EXPECT_EQ(phase_t::reserved, controller->GetPhase());

	ON_CALL(*bus, GetDAT).WillByDefault(Return(1));
	controller->Selection();
	EXPECT_EQ(phase_t::reserved, controller->GetPhase()) << "There is no device that can be selected";

	auto device = make_shared<MockPrimaryDevice>(0);
	controller->AddDevice(device);
	EXPECT_CALL(*bus, SetBSY(true));
	controller->Selection();
	EXPECT_EQ(phase_t::selection, controller->GetPhase());
}

TEST(ScsiControllerTest, Command)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::command);
	EXPECT_CALL(controller, Status);
	controller.Command();
	EXPECT_EQ(phase_t::command, controller.GetPhase());

	controller.SetPhase(phase_t::reserved);
	EXPECT_CALL(*bus, SetMSG(false));
	EXPECT_CALL(*bus, SetCD(true));
	EXPECT_CALL(*bus, SetIO(false));
	controller.Command();
	EXPECT_EQ(phase_t::command, controller.GetPhase());

	controller.SetPhase(phase_t::reserved);
	ON_CALL(*bus, CommandHandShake).WillByDefault(Return(6));
	EXPECT_CALL(*bus, SetMSG(false));
	EXPECT_CALL(*bus, SetCD(true));
	EXPECT_CALL(*bus, SetIO(false));
	EXPECT_CALL(controller, Execute);
	controller.Command();
	EXPECT_EQ(phase_t::command, controller.GetPhase());
}

TEST(ScsiControllerTest, MsgIn)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::reserved);
	EXPECT_CALL(*bus, SetMSG(true));
	EXPECT_CALL(*bus, SetCD(true));
	EXPECT_CALL(*bus, SetIO(true));
	controller.MsgIn();
	EXPECT_EQ(phase_t::msgin, controller.GetPhase());
	EXPECT_FALSE(controller.HasValidLength());
	EXPECT_EQ(0, controller.GetOffset());
}

TEST(ScsiControllerTest, MsgOut)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::reserved);
	EXPECT_CALL(*bus, SetMSG(true));
	EXPECT_CALL(*bus, SetCD(true));
	EXPECT_CALL(*bus, SetIO(false));
	controller.MsgOut();
	EXPECT_EQ(phase_t::msgout, controller.GetPhase());
	EXPECT_EQ(1, controller.GetLength());
	EXPECT_EQ(0, controller.GetOffset());
}

TEST(ScsiControllerTest, DataIn)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::reserved);
	controller.SetLength(0);
	EXPECT_CALL(controller, Status);
	controller.DataIn();
	EXPECT_EQ(phase_t::reserved, controller.GetPhase());

	controller.SetLength(1);
	EXPECT_CALL(*bus, SetMSG(false));
	EXPECT_CALL(*bus, SetCD(false));
	EXPECT_CALL(*bus, SetIO(true));
	controller.DataIn();
	EXPECT_EQ(phase_t::datain, controller.GetPhase());
	EXPECT_EQ(0, controller.GetOffset());
}

TEST(ScsiControllerTest, DataOut)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	controller.SetPhase(phase_t::reserved);
	controller.SetLength(0);
	EXPECT_CALL(controller, Status);
	controller.DataOut();
	EXPECT_EQ(phase_t::reserved, controller.GetPhase());

	controller.SetLength(1);
	EXPECT_CALL(*bus, SetMSG(false));
	EXPECT_CALL(*bus, SetCD(false));
	EXPECT_CALL(*bus, SetIO(false));
	controller.DataOut();
	EXPECT_EQ(phase_t::dataout, controller.GetPhase());
	EXPECT_EQ(0, controller.GetOffset());
}

TEST(ScsiControllerTest, Error)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	MockScsiController controller(controller_manager, 0);

	ON_CALL(*bus, GetRST).WillByDefault(Return(true));
	controller.SetPhase(phase_t::reserved);
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST());
	EXPECT_CALL(*bus, Reset);
	EXPECT_CALL(controller, Reset);
	controller.Error(sense_key::ABORTED_COMMAND, asc::NO_ADDITIONAL_SENSE_INFORMATION, status::RESERVATION_CONFLICT);
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_EQ(phase_t::reserved, controller.GetPhase());

	ON_CALL(*bus, GetRST).WillByDefault(Return(false));
	controller.SetPhase(phase_t::status);
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST());
	EXPECT_CALL(*bus, Reset).Times(0);
	EXPECT_CALL(controller, Reset).Times(0);
	controller.Error(sense_key::ABORTED_COMMAND, asc::NO_ADDITIONAL_SENSE_INFORMATION, status::RESERVATION_CONFLICT);
	EXPECT_EQ(phase_t::busfree, controller.GetPhase());

	controller.SetPhase(phase_t::msgin);
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST());
	EXPECT_CALL(*bus, Reset).Times(0);
	EXPECT_CALL(controller, Reset).Times(0);
	controller.Error(sense_key::ABORTED_COMMAND, asc::NO_ADDITIONAL_SENSE_INFORMATION, status::RESERVATION_CONFLICT);
	EXPECT_EQ(phase_t::busfree, controller.GetPhase());

	controller.SetPhase(phase_t::reserved);
	EXPECT_CALL(*bus, Acquire);
	EXPECT_CALL(*bus, GetRST());
	EXPECT_CALL(*bus, Reset).Times(0);
	EXPECT_CALL(controller, Reset).Times(0);
	EXPECT_CALL(controller, Status);
	controller.Error(sense_key::ABORTED_COMMAND, asc::NO_ADDITIONAL_SENSE_INFORMATION, status::RESERVATION_CONFLICT);
	EXPECT_EQ(status::RESERVATION_CONFLICT, controller.GetStatus());
	EXPECT_EQ(phase_t::reserved, controller.GetPhase());
}

TEST(ScsiControllerTest, RequestSense)
{
	auto bus = make_shared<NiceMock<MockBus>>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockScsiController>(controller_manager, 0);
	auto device = make_shared<MockPrimaryDevice>(0);
	const unordered_map<string, string> params;
	device->Init(params);

	controller->AddDevice(device);

	auto& cmd = controller->GetCmd();
	// ALLOCATION LENGTH
	cmd[4] = 255;
	// Non-existing LUN
	cmd[1] = 0x20;

	device->SetReady(true);
	EXPECT_CALL(*controller, Status);
	device->Dispatch(scsi_command::eCmdRequestSense);
	EXPECT_EQ(status::GOOD, controller->GetStatus()) << "Wrong CHECK CONDITION for non-existing LUN";
}
