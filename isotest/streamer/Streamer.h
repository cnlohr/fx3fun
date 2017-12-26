#pragma once
#include "CyAPI.h"
#include <dbt.h>
#undef MessageBox

namespace Streams
{
    using namespace System;
    using namespace System::ComponentModel;
    using namespace System::Collections;
    using namespace System::Windows::Forms;
    using namespace System::Data;
    using namespace System::Drawing;
    using namespace System::Threading;
    using namespace System::Diagnostics;
    using namespace System::Reflection;


    public __gc class Form1 : public System::Windows::Forms::Form
    {	

    private: System::Windows::Forms::TextBox *  TimeOutBox;
    private: System::Windows::Forms::CheckBox *  ShowDataBox;


    private: System::Windows::Forms::Label *  label1;
    private: System::Windows::Forms::Label *  label2;
    private: System::Windows::Forms::ComboBox *  PacketsPerXferBox;
    private: System::Windows::Forms::Label *  label4;
    private: System::Windows::Forms::Label *  label5;
    private: System::Windows::Forms::Button *  StartBtn;
    private: System::Windows::Forms::ComboBox *  EndPointsBox;

    private: System::Windows::Forms::ComboBox *  QueueLenBox;
    private: System::Windows::Forms::TextBox *  DataTextBox;
    private: System::Windows::Forms::Label *  RateLabel;
    private: System::Windows::Forms::ProgressBar *  RateProgressBar;
    private: System::Windows::Forms::TextBox *  SuccessesBox;
    private: System::Windows::Forms::TextBox *  FailuresBox;
    private: System::Windows::Forms::Label *  SuccessLabel;
    private: System::Windows::Forms::GroupBox *  RateGroupBox;
    private: System::Windows::Forms::Label*  label15;
    private: System::Windows::Forms::ComboBox*  DeviceComboBox;






    private: System::Windows::Forms::Label *  FailureLabel;


    public:	

        Form1(void)
        {
            InitializeComponent();

            bPnP_Arrival		= false;
            bPnP_Removal		= false;
            bPnP_DevNodeChange	= false;
            bAppQuiting         = false;
            bDeviceRefreshNeeded = false;
        }



    private:

        System::ComponentModel::IContainer *  components;

        void InitializeComponent(void)
        {
            System::ComponentModel::ComponentResourceManager*  resources = (new System::ComponentModel::ComponentResourceManager(__typeof(Form1)));
            this->EndPointsBox = (new System::Windows::Forms::ComboBox());
            this->label1 = (new System::Windows::Forms::Label());
            this->label2 = (new System::Windows::Forms::Label());
            this->PacketsPerXferBox = (new System::Windows::Forms::ComboBox());
            this->label4 = (new System::Windows::Forms::Label());
            this->TimeOutBox = (new System::Windows::Forms::TextBox());
            this->label5 = (new System::Windows::Forms::Label());
            this->StartBtn = (new System::Windows::Forms::Button());
            this->DataTextBox = (new System::Windows::Forms::TextBox());
            this->ShowDataBox = (new System::Windows::Forms::CheckBox());
            this->RateGroupBox = (new System::Windows::Forms::GroupBox());
            this->RateLabel = (new System::Windows::Forms::Label());
            this->RateProgressBar = (new System::Windows::Forms::ProgressBar());
            this->QueueLenBox = (new System::Windows::Forms::ComboBox());
            this->SuccessesBox = (new System::Windows::Forms::TextBox());
            this->FailuresBox = (new System::Windows::Forms::TextBox());
            this->SuccessLabel = (new System::Windows::Forms::Label());
            this->FailureLabel = (new System::Windows::Forms::Label());
            this->label15 = (new System::Windows::Forms::Label());
            this->DeviceComboBox = (new System::Windows::Forms::ComboBox());
            this->RateGroupBox->SuspendLayout();
            this->SuspendLayout();
            // 
            // EndPointsBox
            // 
            this->EndPointsBox->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            this->EndPointsBox->Location = System::Drawing::Point(136, 53);
            this->EndPointsBox->MaxDropDownItems = 16;
            this->EndPointsBox->Name = S"EndPointsBox";
            this->EndPointsBox->Size = System::Drawing::Size(270, 21);
            this->EndPointsBox->Sorted = true;
            this->EndPointsBox->TabIndex = 3;
            this->EndPointsBox->SelectionChangeCommitted += new System::EventHandler(this, &Form1::EndPointsBox_SelectedIndexChanged);
            // 
            // label1
            // 
            this->label1->Location = System::Drawing::Point(17, 58);
            this->label1->Name = S"label1";
            this->label1->Size = System::Drawing::Size(100, 16);
            this->label1->TabIndex = 2;
            this->label1->Text = S"Endpoint . . . . . ";
            this->label1->TextAlign = System::Drawing::ContentAlignment::BottomLeft;
            // 
            // label2
            // 
            this->label2->Location = System::Drawing::Point(17, 91);
            this->label2->Name = S"label2";
            this->label2->Size = System::Drawing::Size(100, 16);
            this->label2->TabIndex = 4;
            this->label2->Text = S"Packets per Xfer";
            this->label2->TextAlign = System::Drawing::ContentAlignment::BottomLeft;
            // 
            // PacketsPerXferBox
            // 
            this->PacketsPerXferBox->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            System::Object* __mcTemp__1[] = new System::Object*[10];
            __mcTemp__1[0] = S"1";
            __mcTemp__1[1] = S"2";
            __mcTemp__1[2] = S"4";
            __mcTemp__1[3] = S"8";
            __mcTemp__1[4] = S"16";
            __mcTemp__1[5] = S"32";
            __mcTemp__1[6] = S"64";
            __mcTemp__1[7] = S"128";
            __mcTemp__1[8] = S"256";
            __mcTemp__1[9] = S"512";
            this->PacketsPerXferBox->Items->AddRange(__mcTemp__1);
            this->PacketsPerXferBox->Location = System::Drawing::Point(136, 86);
            this->PacketsPerXferBox->Name = S"PacketsPerXferBox";
            this->PacketsPerXferBox->Size = System::Drawing::Size(76, 21);
            this->PacketsPerXferBox->TabIndex = 5;
            // 
            // label4
            // 
            this->label4->Location = System::Drawing::Point(17, 123);
            this->label4->Name = S"label4";
            this->label4->Size = System::Drawing::Size(88, 16);
            this->label4->TabIndex = 6;
            this->label4->Text = S"Xfers to Queue";
            this->label4->TextAlign = System::Drawing::ContentAlignment::BottomLeft;
            // 
            // TimeOutBox
            // 
            this->TimeOutBox->Location = System::Drawing::Point(136, 154);
            this->TimeOutBox->Name = S"TimeOutBox";
            this->TimeOutBox->Size = System::Drawing::Size(76, 20);
            this->TimeOutBox->TabIndex = 9;
            this->TimeOutBox->Text = S"1500";
            this->TimeOutBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
            // 
            // label5
            // 
            this->label5->Location = System::Drawing::Point(17, 154);
            this->label5->Name = S"label5";
            this->label5->Size = System::Drawing::Size(115, 16);
            this->label5->TabIndex = 8;
            this->label5->Text = S"Timeout Per Xfer (ms)";
            this->label5->TextAlign = System::Drawing::ContentAlignment::BottomLeft;
            // 
            // StartBtn
            // 
            this->StartBtn->BackColor = System::Drawing::Color::Aquamarine;
            this->StartBtn->Location = System::Drawing::Point(226, 149);
            this->StartBtn->Name = S"StartBtn";
            this->StartBtn->Size = System::Drawing::Size(180, 28);
            this->StartBtn->TabIndex = 10;
            this->StartBtn->Text = S"Start";
            this->StartBtn->UseVisualStyleBackColor = false;
            this->StartBtn->Click += new System::EventHandler(this, &Form1::StartBtn_Click);
            // 
            // DataTextBox
            // 
            this->DataTextBox->Font = (new System::Drawing::Font(S"Courier New", 8.25F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
                (System::Byte)0));
            this->DataTextBox->Location = System::Drawing::Point(17, 295);
            this->DataTextBox->Multiline = true;
            this->DataTextBox->Name = S"DataTextBox";
            this->DataTextBox->Size = System::Drawing::Size(389, 112);
            this->DataTextBox->TabIndex = 18;
            this->DataTextBox->TabStop = false;
            // 
            // ShowDataBox
            // 
            this->ShowDataBox->Location = System::Drawing::Point(17, 271);
            this->ShowDataBox->Name = S"ShowDataBox";
            this->ShowDataBox->Size = System::Drawing::Size(144, 16);
            this->ShowDataBox->TabIndex = 17;
            this->ShowDataBox->Text = S"Show Transfered Data";
            // 
            // RateGroupBox
            // 
            this->RateGroupBox->Controls->Add(this->RateLabel);
            this->RateGroupBox->Controls->Add(this->RateProgressBar);
            this->RateGroupBox->Location = System::Drawing::Point(17, 182);
            this->RateGroupBox->Name = S"RateGroupBox";
            this->RateGroupBox->Size = System::Drawing::Size(389, 72);
            this->RateGroupBox->TabIndex = 15;
            this->RateGroupBox->TabStop = false;
            this->RateGroupBox->Text = S" Transfer Rate (KB/s) ";
            // 
            // RateLabel
            // 
            this->RateLabel->Font = (new System::Drawing::Font(S"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
                (System::Byte)0));
            this->RateLabel->Location = System::Drawing::Point(155, 48);
            this->RateLabel->Name = S"RateLabel";
            this->RateLabel->Size = System::Drawing::Size(74, 16);
            this->RateLabel->TabIndex = 1;
            this->RateLabel->Text = S"0";
            this->RateLabel->TextAlign = System::Drawing::ContentAlignment::BottomCenter;
            // 
            // RateProgressBar
            // 
            this->RateProgressBar->Location = System::Drawing::Point(18, 24);
            this->RateProgressBar->Maximum = 800000;
            this->RateProgressBar->Name = S"RateProgressBar";
            this->RateProgressBar->Size = System::Drawing::Size(350, 16);
            this->RateProgressBar->TabIndex = 0;
            // 
            // QueueLenBox
            // 
            this->QueueLenBox->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            System::Object* __mcTemp__2[] = new System::Object*[7];
            __mcTemp__2[0] = S"1";
            __mcTemp__2[1] = S"2";
            __mcTemp__2[2] = S"4";
            __mcTemp__2[3] = S"8";
            __mcTemp__2[4] = S"16";
            __mcTemp__2[5] = S"32";
            __mcTemp__2[6] = S"64";
            this->QueueLenBox->Items->AddRange(__mcTemp__2);
            this->QueueLenBox->Location = System::Drawing::Point(136, 118);
            this->QueueLenBox->Name = S"QueueLenBox";
            this->QueueLenBox->Size = System::Drawing::Size(76, 21);
            this->QueueLenBox->TabIndex = 7;
            // 
            // SuccessesBox
            // 
            this->SuccessesBox->Location = System::Drawing::Point(298, 87);
            this->SuccessesBox->Name = S"SuccessesBox";
            this->SuccessesBox->Size = System::Drawing::Size(108, 20);
            this->SuccessesBox->TabIndex = 12;
            this->SuccessesBox->Text = S"0";
            this->SuccessesBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
            // 
            // FailuresBox
            // 
            this->FailuresBox->Location = System::Drawing::Point(298, 119);
            this->FailuresBox->Name = S"FailuresBox";
            this->FailuresBox->Size = System::Drawing::Size(108, 20);
            this->FailuresBox->TabIndex = 14;
            this->FailuresBox->Text = S"0";
            this->FailuresBox->TextAlign = System::Windows::Forms::HorizontalAlignment::Right;
            // 
            // SuccessLabel
            // 
            this->SuccessLabel->Location = System::Drawing::Point(223, 90);
            this->SuccessLabel->Name = S"SuccessLabel";
            this->SuccessLabel->Size = System::Drawing::Size(64, 16);
            this->SuccessLabel->TabIndex = 11;
            this->SuccessLabel->Text = S"Successes";
            this->SuccessLabel->TextAlign = System::Drawing::ContentAlignment::BottomLeft;
            // 
            // FailureLabel
            // 
            this->FailureLabel->Location = System::Drawing::Point(223, 123);
            this->FailureLabel->Name = S"FailureLabel";
            this->FailureLabel->Size = System::Drawing::Size(64, 16);
            this->FailureLabel->TabIndex = 13;
            this->FailureLabel->Text = S"Failures";
            this->FailureLabel->TextAlign = System::Drawing::ContentAlignment::BottomLeft;
            // 
            // label15
            // 
            this->label15->AutoSize = true;
            this->label15->Location = System::Drawing::Point(17, 24);
            this->label15->Name = S"label15";
            this->label15->Size = System::Drawing::Size(101, 13);
            this->label15->TabIndex = 0;
            this->label15->Text = S"Connected Devices";
            // 
            // DeviceComboBox
            // 
            this->DeviceComboBox->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            this->DeviceComboBox->FormattingEnabled = true;
            this->DeviceComboBox->Location = System::Drawing::Point(136, 19);
            this->DeviceComboBox->Name = S"DeviceComboBox";
            this->DeviceComboBox->Size = System::Drawing::Size(270, 21);
            this->DeviceComboBox->TabIndex = 1;
            this->DeviceComboBox->SelectionChangeCommitted += new System::EventHandler(this, &Form1::DeviceComboBox_SelectedIndexChanged);
            // 
            // Form1
            // 
            this->AutoScaleBaseSize = System::Drawing::Size(5, 13);
            this->ClientSize = System::Drawing::Size(416, 426);
            this->Controls->Add(this->DeviceComboBox);
            this->Controls->Add(this->label15);
            this->Controls->Add(this->FailureLabel);
            this->Controls->Add(this->SuccessLabel);
            this->Controls->Add(this->FailuresBox);
            this->Controls->Add(this->SuccessesBox);
            this->Controls->Add(this->QueueLenBox);
            this->Controls->Add(this->RateGroupBox);
            this->Controls->Add(this->DataTextBox);
            this->Controls->Add(this->ShowDataBox);
            this->Controls->Add(this->StartBtn);
            this->Controls->Add(this->label5);
            this->Controls->Add(this->TimeOutBox);
            this->Controls->Add(this->label4);
            this->Controls->Add(this->PacketsPerXferBox);
            this->Controls->Add(this->label2);
            this->Controls->Add(this->label1);
            this->Controls->Add(this->EndPointsBox);
            this->Icon = (__try_cast<System::Drawing::Icon*  >(resources->GetObject(S"$this.Icon")));
            this->Name = S"Form1";
            this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
            this->Text = S"C++ Streamer";
            this->Load += new System::EventHandler(this, &Form1::Form1_Load);
            this->Closed += new System::EventHandler(this, &Form1::Form1_Closed);
            this->RateGroupBox->ResumeLayout(false);
            this->ResumeLayout(false);
            this->PerformLayout();

        }	

     
        Thread						*XferThread;

        CCyUSBDevice				*USBDevice;

        static const int				MAX_QUEUE_SZ = 64;
        static const int				VENDOR_ID	= 0x04B4;
        static const int				PRODUCT_ID	= 0x00F1; 

        // These declared static because accessed from the static XferLoop method
        // XferLoop is static because it is used as a separate thread.

        static CCyUSBEndPoint		*EndPt;
        static int					PPX;
        static int					QueueSize;
        static int					TimeOut;
        static bool					bShowData;
        static bool					bStreaming;
        static bool                 bDeviceRefreshNeeded;
        static bool                 bAppQuiting;
        static bool					bHighSpeedDevice;
        static bool					bSuperSpeedDevice;

        static ProgressBar			*XferRateBar;
        static Label				*XferRateLabel;
        static TextBox				*DataBox;
        static TextBox				*SuccessBox;
        static TextBox				*FailureBox;

        static ComboBox				*EptsBox;
        static ComboBox				*PpxBox;
        static ComboBox				*QueueBox;
        static Button				*StartButton;
        static TextBox				*TimeoutBox;
        static CheckBox				*ShowBox;

        bool						bPnP_Arrival;
        bool						bPnP_Removal;
        bool						bPnP_DevNodeChange;


        void GetStreamerDevice()
        {
            StartBtn->Enabled = false;

            EndPointsBox->Items->Clear();
            EndPointsBox->Text = "";

            USBDevice = new CCyUSBDevice((HANDLE)this->Handle,CYUSBDRV_GUID,true);

            if (USBDevice == NULL) return;

            int n = USBDevice->DeviceCount();
            DeviceComboBox->Items->Clear();

            /////////////////////////////////////////////////////////////////
            // Walk through all devices looking for VENDOR_ID/PRODUCT_ID
            // We No longer got restricted with vendor ID and Product ID.
            // Check for vendor ID and product ID is discontinued.
            ///////////////////////////////////////////////////////////////////
            for (int i=0; i<n; i++)
            {
                //if ((USBDevice->VendorID == VENDOR_ID) && (USBDevice->ProductID == PRODUCT_ID)) 
                //    break;
                USBDevice->Open(i);
                String *strDeviceData = "";
                strDeviceData = String::Concat(strDeviceData, "(0x");
                strDeviceData = String::Concat(strDeviceData, USBDevice->VendorID.ToString("X4"));
                strDeviceData = String::Concat(strDeviceData, " - 0x");
                strDeviceData = String::Concat(strDeviceData, USBDevice->ProductID.ToString("X4"));
                strDeviceData = String::Concat(strDeviceData, ") ");
                strDeviceData = String::Concat(strDeviceData, USBDevice->FriendlyName);               
                
                DeviceComboBox->Items->Add(strDeviceData);      
                DeviceComboBox->Enabled = true;
            }
            if (n > 0 ) {
                DeviceComboBox->SelectedIndex = 0;
                USBDevice->Open(0);
            }


            //if ((USBDevice->VendorID == VENDOR_ID) && (USBDevice->ProductID == PRODUCT_ID)) 
            {
                StartBtn->Enabled = true;

                int interfaces = USBDevice->AltIntfcCount()+1;

                bHighSpeedDevice = USBDevice->bHighSpeed;
                bSuperSpeedDevice = USBDevice->bSuperSpeed;

                for (int i=0; i< interfaces; i++)
                {
                    if (USBDevice->SetAltIntfc(i) == true )
                    {

                        int eptCnt = USBDevice->EndPointCount();

                        // Fill the EndPointsBox
                        for (int e=1; e<eptCnt; e++)
                        {
                            CCyUSBEndPoint *ept = USBDevice->EndPoints[e];
                            // INTR, BULK and ISO endpoints are supported.
                            if ((ept->Attributes >= 1) && (ept->Attributes <= 3))
                            {
                                String *s = "";
                                s = String::Concat(s, ((ept->Attributes == 1) ? "ISOC " :
                                       ((ept->Attributes == 2) ? "BULK " : "INTR ")));
                                s = String::Concat(s, ept->bIn ? "IN,       " : "OUT,   ");
                                s = String::Concat(s, ept->MaxPktSize.ToString(), " Bytes,");
                                if(USBDevice->BcdUSB == USB30MAJORVER)
                                    s = String::Concat(s, ept->ssmaxburst.ToString(), " MaxBurst,");

                                s = String::Concat(s, "   (", i.ToString(), " - ");
                                s = String::Concat(s, "0x", ept->Address.ToString("X02"), ")");
                                EndPointsBox->Items->Add(s);
                            }
                        }
                    }
                }
                if (EndPointsBox->Items->Count > 0 )
                    EndPointsBox->SelectedIndex = 0;
                else
                    StartBtn->Enabled = false;
            }
        }



        void Form1_Load(System::Object *  sender, System::EventArgs *  e)
        {
            XferRateBar		= RateProgressBar;
            XferRateLabel	= RateLabel;
            DataBox			= DataTextBox;
            SuccessBox		= SuccessesBox;
            FailureBox		= FailuresBox;

            EptsBox			= EndPointsBox;
            PpxBox			= PacketsPerXferBox;
            QueueBox		= QueueLenBox;
            StartButton		= StartBtn;
            TimeoutBox		= TimeOutBox;
            ShowBox			= ShowDataBox;
            bDeviceRefreshNeeded = false;

            if (PacketsPerXferBox->SelectedIndex == -1 ) PacketsPerXferBox->SelectedIndex = 5;
            if (QueueLenBox->SelectedIndex == -1 ) QueueLenBox->SelectedIndex = 4;

            GetStreamerDevice();

            XferThread = new Thread(new ThreadStart(0,&XferLoop));
        }


        void Form1_Closed(System::Object *  sender, System::EventArgs *  e)
        {
            //if (XferThread->ThreadState == System::Threading::ThreadState::Suspended)
            //XferThread->Resume();

            bAppQuiting = true;
            bStreaming = false;  // Stop the thread's xfer loop     
            bDeviceRefreshNeeded = false;

            if (XferThread->ThreadState == System::Threading::ThreadState::Running)
                XferThread->Join(10);
        }


        void StartBtn_Click(System::Object *  sender, System::EventArgs *  e)
        {
            Decimal db;
            int ab;

            if(!Decimal::TryParse(this->TimeOutBox->Text, &db))
            {
                ::MessageBox(NULL,"Invalid Input : TimeOut Per Xfer(ms)","Streamer",0);				
                this->TimeOutBox->Text = "";
                return;				
            }
            if(!Int32::TryParse(this->TimeOutBox->Text,&ab))			
            {
                ::MessageBox(NULL,"Invalid Input : TimeOut Per Xfer(ms)","Streamer",0);				
                this->TimeOutBox->Text = "";
                return;
            }

            if (XferThread) {
                switch (XferThread->ThreadState)
                {
                case System::Threading::ThreadState::Stopped:
                case System::Threading::ThreadState::Unstarted:

                    if (EndPt == NULL ) EndPointsBox_SelectedIndexChanged(NULL, NULL);
                    else EnforceValidPPX();

                    StartBtn->Text = "Stop";
                    SuccessBox->Text = "";
                    FailureBox->Text = "";
                    StartBtn->BackColor = Color::MistyRose;
                    StartBtn->Refresh();

                    bStreaming = true;

                    // Start-over, initializing counters, etc.
                    if ((XferThread->ThreadState) == System::Threading::ThreadState::Stopped)
                        XferThread = new Thread(new ThreadStart(0,&XferLoop));

                    PPX = Convert::ToInt32(PacketsPerXferBox->Text);

                    QueueSize = Convert::ToInt32(QueueLenBox->Text);
                    TimeOut = Convert::ToInt32(TimeOutBox->Text);
                    bShowData = ShowDataBox->Checked;

                    EndPointsBox->Enabled		= false;
                    PacketsPerXferBox->Enabled	= false;
                    QueueLenBox->Enabled		= false;
                    TimeOutBox->Enabled			= false;
                    ShowDataBox->Enabled		= false;
                    DeviceComboBox->Enabled     = false;

                    XferThread->Start();
                    break;
                case System::Threading::ThreadState::Running:
                    StartBtn->Text = "Start";
                    StartBtn->BackColor = Color::Aquamarine;
                    StartBtn->Refresh();

                    bStreaming = false;  // Stop the thread's xfer loop
                    XferThread->Join(10);

                    EndPointsBox->Enabled		= true;
                    PacketsPerXferBox->Enabled	= true;
                    QueueLenBox->Enabled		= true;
                    TimeOutBox->Enabled			= true;
                    ShowDataBox->Enabled		= true;
                    DeviceComboBox->Enabled     = true;

                    if (bDeviceRefreshNeeded == true )
                    {
                        bDeviceRefreshNeeded = false;
                        GetStreamerDevice();                        
                    }

                    break;
                }

            } 

        }
        
        void DeviceComboBox_SelectedIndexChanged(System::Object *  sender, System::EventArgs *  e)
        {
            if (DeviceComboBox->SelectedIndex == -1 ) return;

            if (USBDevice->IsOpen() == true) USBDevice->Close();
            USBDevice->Open(DeviceComboBox->SelectedIndex);           

            int interfaces = USBDevice->AltIntfcCount()+1;

            bHighSpeedDevice = USBDevice->bHighSpeed;
            bSuperSpeedDevice = USBDevice->bSuperSpeed;

            EndPointsBox->Items->Clear();

            for (int i=0; i< interfaces; i++)
            {
                if (USBDevice->SetAltIntfc(i) == true )
                {

                    int eptCnt = USBDevice->EndPointCount();

                    // Fill the EndPointsBox
                    for (int e=1; e<eptCnt; e++)
                    {
                        CCyUSBEndPoint *ept = USBDevice->EndPoints[e];
                        // INTR, BULK and ISO endpoints are supported.
                        if ((ept->Attributes >= 1) && (ept->Attributes <= 3))
                        {
                            String *s = "";
                            s = String::Concat(s, ((ept->Attributes == 1) ? "ISOC " :
                                   ((ept->Attributes == 2) ? "BULK " : "INTR ")));
                            s = String::Concat(s, ept->bIn ? "IN,       " : "OUT,   ");
                            s = String::Concat(s, ept->MaxPktSize.ToString(), " Bytes,");
                            if(USBDevice->BcdUSB == USB30MAJORVER)
                                s = String::Concat(s, ept->ssmaxburst.ToString(), " MaxBurst,");

                            s = String::Concat(s, "   (", i.ToString(), " - ");
                            s = String::Concat(s, "0x", ept->Address.ToString("X02"), ")");
                            EndPointsBox->Items->Add(s);
                        }
                    }
                }
            }
            if (EndPointsBox->Items->Count > 0 )
            {
                EndPointsBox->SelectedIndex = 0;
                EndPointsBox_SelectedIndexChanged(NULL, NULL);
                StartBtn->Enabled = true;
            }
            else
                StartBtn->Enabled = false;
        }

        void EndPointsBox_SelectedIndexChanged(System::Object *  sender, System::EventArgs *  e)
        {
            // Parse the alt setting and endpoint address from the EndPointsBox->Text
            String *tmp = EndPointsBox->Text->Substring(EndPointsBox->Text->IndexOf("("),10);
            int  alt = Convert::ToInt32(tmp->Substring(1,1));

            String *addr = tmp->Substring(7,2);
            //changed int to __int64 to avoid data loss
            __int64 eptAddr = HexToInt(addr);

            int clrAlt = (USBDevice->AltIntfc() == 0) ? 1 : 0;

            // Attempt to set the selected Alt setting and get the endpoint
            if (! USBDevice->SetAltIntfc(alt))
            {
                MessageBox::Show("Alt interface could not be selected.","USB Exception",MessageBoxButtons::OK,MessageBoxIcon::Hand);
                StartBtn->Enabled = false;
                USBDevice->SetAltIntfc(clrAlt); // Cleans-up
                return;
            }


            EndPt = USBDevice->EndPointOf((UCHAR)eptAddr);

            StartBtn->Enabled = true;


            if (EndPt->Attributes == 1)
            {
                SuccessLabel->Text = "Good Pkts";
                FailureLabel->Text = "Bad Pkts";
            }
            else
            {
                SuccessLabel->Text = "Successes";
                FailureLabel->Text = "Failures";
            }

            EnforceValidPPX();
        }


        void EnforceValidPPX()
        {
            if (PacketsPerXferBox->SelectedIndex == -1 ) PacketsPerXferBox->SelectedIndex = 5;
            if (QueueLenBox->SelectedIndex == -1 ) QueueLenBox->SelectedIndex = 4;
            PPX = Convert::ToInt32(PacketsPerXferBox->Text);

            if(EndPt->MaxPktSize==0)
				return;

            // Limit total transfer length to 4MByte
            int len = ((EndPt->MaxPktSize) * PPX);

            int maxLen = 0x400000;  //4MByte
            if (len > maxLen)
            {

                PPX = maxLen / (EndPt->MaxPktSize);
                if((PPX%8)!=0)
                    PPX -= (PPX%8);

                int iIndex = PacketsPerXferBox->SelectedIndex;
                PacketsPerXferBox->Items->Remove(PacketsPerXferBox->Text);
                PacketsPerXferBox->Items->Insert(iIndex,PPX.ToString());
                PacketsPerXferBox->SelectedIndex = iIndex;

                DataBox->Text = String::Concat(DataBox->Text,"Total xfer length limited to 4Mbyte.\r\n");
                DataBox->Text = String::Concat(DataBox->Text,"Packets per Xfer has been adjusted.\r\n");
                DataBox->SelectionStart = DataBox->Text->Length;
                DataBox->ScrollToCaret();
            }

            if ((bSuperSpeedDevice || bHighSpeedDevice) && (EndPt->Attributes == 1))  // HS/SS ISOC Xfers must use PPX >= 8
            {
                if (PPX < 8)
                {
                    PPX = 8;
                    Display("ISOC xfers require at least 8 Packets per Xfer.");
                    Display("Packets per Xfer has been adjusted.");
                }

                PPX = (PPX / 8) * 8;

                if(bHighSpeedDevice)
                {
                    if(PPX >128)
                    {					
                        PPX = 128;
                        Display("Hish Speed ISOC xfers does not support more than 128 Packets per transfer");						
                    }
                }
            }

            PacketsPerXferBox->Text = PPX.ToString();			
        }


        static UInt64 HexToInt(String *hexString)
        {
            String *HexChars = "0123456789abcdef";

            String *s = hexString->ToLower();

            // Trim off the 0x prefix
            if (s->Length > 2)
                if (s->Substring(0,2)->Equals("0x"))
                    s = s->Substring(2,s->Length-2);


            String *_s = "";
            int len = s->Length;

            // Reverse the digits
            for (int i=len-1; i>=0; i--) _s  = String::Concat(_s,s->Substring(i,1));

            UInt64 sum = 0;
            UInt64 pwrF = 1;
            for (int i=0; i<len; i++)
            {
                UInt32 ordinal = (UInt32) HexChars->IndexOf(_s->Substring(i,1));
                sum += (i==0) ? ordinal : pwrF*ordinal;
                pwrF *= 16;
            }


            return sum;
        }

        static void Display(String *s)
        {
            DataBox->Text = String::Concat(DataBox->Text, s, "\r\n");
            DataBox->SelectionStart = DataBox->Text->Length;
            DataBox->ScrollToCaret();
        }

        static void Display16Bytes(PUCHAR data)
        {
            String *xData = "";

            for (int i=0; i<16; i++) 
                xData = String::Concat(xData,data[i].ToString("X02"), " ");

            Display(xData);
        } 




        // This method executes in it's own thread.  The thread gets re-launched each time the
        // Start button is clicked (and the thread isn't already running).
        static void XferLoop()
        {
            long BytesXferred = 0;
            unsigned long Successes = 0;
            unsigned long Failures = 0;
            int i = 0;

            // Allocate the arrays needed for queueing
            PUCHAR			*buffers		= new PUCHAR[QueueSize];
            CCyIsoPktInfo	**isoPktInfos	= new CCyIsoPktInfo*[QueueSize];
            PUCHAR			*contexts		= new PUCHAR[QueueSize];
            OVERLAPPED		inOvLap[MAX_QUEUE_SZ];

            long len = EndPt->MaxPktSize * PPX; // Each xfer request will get PPX isoc packets

            EndPt->SetXferSize(len);

            // Allocate all the buffers for the queues
            for (i=0; i< QueueSize; i++) 
            { 
                buffers[i]        = new UCHAR[len];
                isoPktInfos[i]    = new CCyIsoPktInfo[PPX];
                inOvLap[i].hEvent = CreateEvent(NULL, false, false, NULL);

                memset(buffers[i],0xEF,len);
            }

            DateTime t1 = DateTime::Now;	// For calculating xfer rate

            // Queue-up the first batch of transfer requests
            for (i=0; i< QueueSize; i++)	
            {
                contexts[i] = EndPt->BeginDataXfer(buffers[i], len, &inOvLap[i]);
                if (EndPt->NtStatus || EndPt->UsbdStatus) // BeginDataXfer failed
                {
                    Display(String::Concat("Xfer request rejected. NTSTATUS = ",EndPt->NtStatus.ToString("x")));
                    AbortXferLoop(i+1, buffers,isoPktInfos,contexts,inOvLap);
                    return;
                }
            }

            i=0;	

            // The infinite xfer loop.
            for (;bStreaming;)		
            {
                long rLen = len;	// Reset this each time through because
                // FinishDataXfer may modify it

                if (!EndPt->WaitForXfer(&inOvLap[i], TimeOut))
                {
                    EndPt->Abort();
                    if (EndPt->LastError == ERROR_IO_PENDING)
                        WaitForSingleObject(inOvLap[i].hEvent,2000);
                }

                if (EndPt->Attributes == 1) // ISOC Endpoint
                {	
                    if (EndPt->FinishDataXfer(buffers[i], rLen, &inOvLap[i], contexts[i], isoPktInfos[i])) 
                    {			
                        CCyIsoPktInfo *pkts = isoPktInfos[i];
                        for (int j=0; j< PPX; j++) 
                        {
							if ((pkts[j].Status == 0) && (pkts[j].Length<=EndPt->MaxPktSize)) 
                            {
                                BytesXferred += pkts[j].Length;

                                if (bShowData)
                                    Display16Bytes(buffers[i]);

                                Successes++;
                            }
                            else
                                Failures++;

                            pkts[j].Length = 0;	// Reset to zero for re-use.
							pkts[j].Status = 0;
                        }

                    } 
                    else
                        Failures++; 

                } 

                else // BULK Endpoint
                {
                    if (EndPt->FinishDataXfer(buffers[i], rLen, &inOvLap[i], contexts[i])) 
                    {			
                        Successes++;
                        BytesXferred += len;

                        if (bShowData)
                            Display16Bytes(buffers[i]);
                    } 
                    else
                        Failures++; 
                }


                if (BytesXferred < 0) // Rollover - reset counters
                {
                    BytesXferred = 0;
                    t1 = DateTime::Now;
                }

                // Re-submit this queue element to keep the queue full
                contexts[i] = EndPt->BeginDataXfer(buffers[i], len, &inOvLap[i]);
                if (EndPt->NtStatus || EndPt->UsbdStatus) // BeginDataXfer failed
                {
                    Display(String::Concat("Xfer request rejected. NTSTATUS = ",EndPt->NtStatus.ToString("x")));
                    AbortXferLoop(QueueSize,buffers,isoPktInfos,contexts,inOvLap);
                    return;
                }

                i++;

                if (i == QueueSize) //Only update the display once each time through the Queue
                {
                    i=0;
                    ShowStats(t1, BytesXferred, Successes, Failures);					
                }

            }  // End of the infinite loop

            // Memory clean-up
            AbortXferLoop(QueueSize,buffers,isoPktInfos,contexts,inOvLap);
        }


        static void AbortXferLoop(int pending, PUCHAR *buffers, CCyIsoPktInfo **isoPktInfos, PUCHAR *contexts, OVERLAPPED inOvLap __nogc [])
        {
            //EndPt->Abort(); - This is disabled to make sure that while application is doing IO and user unplug the device, this function hang the app.
            long len = EndPt->MaxPktSize * PPX;
            EndPt->Abort();

            for (int j=0; j< QueueSize; j++) 
            { 
                if (j<pending)
                {
                    EndPt->WaitForXfer(&inOvLap[j], TimeOut);
                    /*{
                        EndPt->Abort();
                        if (EndPt->LastError == ERROR_IO_PENDING)
                            WaitForSingleObject(inOvLap[j].hEvent,2000);
                    }*/
                    EndPt->FinishDataXfer(buffers[j], len, &inOvLap[j], contexts[j]);
                }

                CloseHandle(inOvLap[j].hEvent);

                delete [] buffers[j];
                delete [] isoPktInfos[j];
            }

            delete [] buffers;
            delete [] isoPktInfos;
            delete [] contexts;


            bStreaming = false;

            if (bAppQuiting == false )
            {
                StartButton->Text = "Start";
                StartButton->BackColor = Color::Aquamarine;
                StartButton->Refresh();

                EptsBox->Enabled	= true;
                PpxBox->Enabled		= true;
                QueueBox->Enabled	= true;
                TimeoutBox->Enabled	= true;
                ShowBox->Enabled	= true;
            }
        }



        static void ShowStats(DateTime t, long bytesXferred, unsigned long successes, unsigned long failures)
        {
            TimeSpan elapsed = DateTime::Now.Subtract(t);

            long totMS = (long)elapsed.TotalMilliseconds;
            if (totMS <= 0)	return;

            long XferRate = bytesXferred / totMS;

            // Convert to KB/s
            XferRate = XferRate * 1000 / 1024;

            // Truncate last 1 or 2 digits
            int rounder = (XferRate > 2000) ? 100 : 10;
            XferRate = XferRate / rounder * rounder;  

			if(XferRate>625000)
				XferRate = 625000;
            //thread safe-commented
            CheckForIllegalCrossThreadCalls = false;

            XferRateBar->Value = XferRate;
            XferRateLabel->Text = XferRate.ToString();

            SuccessBox->Text = successes.ToString();
            FailureBox->Text = failures.ToString();

        }




    protected:	

        [System::Security::Permissions::PermissionSet(System::Security::Permissions::SecurityAction::Demand, Name="FullTrust")]
        virtual void WndProc(Message *m)
        {	
            if (m->Msg == WM_DEVICECHANGE) 
            {
                // Tracks DBT_DEVNODES_CHANGED followed by DBT_DEVICEREMOVECOMPLETE
                if (m->WParam == DBT_DEVNODES_CHANGED) 
                {
                    bPnP_DevNodeChange = true;
                    bPnP_Removal = false;
                }

                // Tracks DBT_DEVICEARRIVAL followed by DBT_DEVNODES_CHANGED
                if (m->WParam == DBT_DEVICEARRIVAL) 
                {
                    bPnP_Arrival = true;
                    bPnP_DevNodeChange = false;
                }

                if (m->WParam == DBT_DEVICEREMOVECOMPLETE) 
                    bPnP_Removal = true;

                // If DBT_DEVICEARRIVAL followed by DBT_DEVNODES_CHANGED
                if (bPnP_DevNodeChange && bPnP_Removal) 
                {
                    bPnP_Removal = false;
                    bPnP_DevNodeChange = false;
                    bDeviceRefreshNeeded = XferThread->IsAlive;
                    
                    if (XferThread->IsAlive == false) 
                    {
                        bStreaming = false;                        
                        if (this->USBDevice) delete this->USBDevice;
                        this->EndPt = NULL;
                        USBDevice = NULL;
                        GetStreamerDevice();
                    }
                    bDeviceRefreshNeeded = XferThread->IsAlive;
                }

                // If DBT_DEVICEARRIVAL followed by DBT_DEVNODES_CHANGED
                if (bPnP_DevNodeChange && bPnP_Arrival) 
                {
                    bPnP_Arrival = false;
                    bPnP_DevNodeChange = false;
                    bDeviceRefreshNeeded = XferThread->IsAlive;
                    if (XferThread->IsAlive == false) 
                    {
                        if (this->USBDevice) delete this->USBDevice;
                        this->EndPt = NULL;
                        USBDevice = NULL;
                        GetStreamerDevice();
                    }
                }

            }

            Form::WndProc(m);
        }







        void Dispose(Boolean disposing)
        {
            if (disposing && components)
            {
                components->Dispose();
            }
            __super::Dispose(disposing);
        }


    };

}


