/*
 * Copyright 2011 Nate Koenig & Andrew Howard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "common/Events.hh"
#include "common/Diagnostics.hh"

#include "gui/PlotPanel.hh"
#include "gui/DiagnosticsDialog.hh"

using namespace gazebo;
using namespace gui;

////////////////////////////////////////////////////////////////////////////////
// Constructor
DiagnosticsDialog::DiagnosticsDialog( wxWindow *parent )
  : wxDialog(parent, wxID_ANY, wxT("Diagnostics"), wxDefaultPosition, wxSize(600,600), wxDEFAULT_FRAME_STYLE)
{
  wxBoxSizer *boxSizer = new wxBoxSizer(wxHORIZONTAL);

  Connect(this->GetId(), wxEVT_INIT_DIALOG, wxInitDialogEventHandler(DiagnosticsDialog::OnInit), NULL, this);

  this->treeCtrl = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(200,100));
  this->treeCtrl->AddRoot( wxT("Timers"), -1, -1, NULL );
  Connect(this->treeCtrl->GetId(), wxEVT_COMMAND_TREE_SEL_CHANGED, wxTreeEventHandler(DiagnosticsDialog::OnTreeClick), NULL, this); 
  boxSizer->Add( this->treeCtrl,0, wxLEFT | wxEXPAND, 5);

  wxBoxSizer *timerBoxSizer = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer *timerNameBox = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText *timerLabelText = new wxStaticText(this, wxID_ANY, wxT("Timer:"), wxDefaultPosition, wxDefaultSize,0);
  this->timerNameCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200,20), 0);
  timerNameBox->Add(timerLabelText, 0, wxLeft |  wxALIGN_CENTER_VERTICAL, 5);
  timerNameBox->Add(this->timerNameCtrl, 0, wxALL | wxALIGN_CENTER_VERTICAL, 1);

  timerBoxSizer->Add(timerNameBox, 0, wxALL);

  wxBoxSizer *timerElapsedBox = new wxBoxSizer(wxHORIZONTAL);
  wxStaticText *timerElapsedText = new wxStaticText(this, wxID_ANY, wxT("Elapsed:"), wxDefaultPosition, wxDefaultSize,0);
  this->timerElapsedCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(200,20) );
  timerElapsedBox->Add(timerElapsedText, 0, wxLeft |  wxALIGN_CENTER_VERTICAL, 5);
  timerElapsedBox->Add(this->timerElapsedCtrl, 0, wxALL | wxALIGN_CENTER_VERTICAL, 1);


  timerBoxSizer->Add(timerElapsedBox, 0, wxALL);

  wxFont graphFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

  this->plot = new PlotPanel(this);
  this->plot->SetLabelX("Time(s)");
  this->plot->SetAxisX(30.0);

  this->plot->SetLabelY("Time(s)");

  timerBoxSizer->Add(this->plot,2, wxEXPAND);

  boxSizer->Add(timerBoxSizer,1, wxALL);
  this->SetSizer( boxSizer );
  this->Layout();

  this->timerStopConnection = event::Events::ConnectDiagTimerStopSignal( boost::bind(&DiagnosticsDialog::TimerStopCB, this, _1) );
}

////////////////////////////////////////////////////////////////////////////////
DiagnosticsDialog::~DiagnosticsDialog()
{
  delete this->treeCtrl;
}

////////////////////////////////////////////////////////////////////////////////
void DiagnosticsDialog::TimerStopCB(std::string timer)
{
  /* NATY: Put back in
  std::map< std::string, std::vector< std::pair<common::Time,common::Time> > >::iterator iter;

  iter = this->times.find(timer);
  if (iter == this->times.end())
  {
    this->treeCtrl->AppendItem( this->treeCtrl->GetRootItem(), 
        wxString::FromAscii(timer.c_str()), -1, -1, NULL );
  }

  common::Time currTime = common::Time::GetRealTime();

  common::Time tm = DiagnosticManager::Instance()->GetTime(timer);
  if (this->times[timer].size() == 0)
  {
    this->times[timer].push_back( std::make_pair(currTime, tm) );
    this->plot->PushData(currTime.Double(), tm.Double(), timer);
  }
  else
  {
    if (currTime - this->times[timer].back().first > common::Time(0,500000))
    {
      this->times[timer].push_back( std::make_pair(currTime, tm) );
      this->plot->PushData(currTime.Double(), tm.Double(), timer);
    }
  }
  */
}

////////////////////////////////////////////////////////////////////////////////
void DiagnosticsDialog::Update()
{
  common::DiagnosticManager::Instance()->SetEnabled(true);

  wxTreeItemId selected = this->treeCtrl->GetSelection();

  this->plot->Refresh();
}

void DiagnosticsDialog::OnTreeClick(wxTreeEvent &event)
{
  wxTreeItemId selected = this->treeCtrl->GetSelection();
  if (selected.IsOk() && selected != this->treeCtrl->GetRootItem())
  {
    wxString selectedName = this->treeCtrl->GetItemText( selected );
    this->plot->AddPlot( std::string( selectedName.mb_str() ) );
  }
}

void DiagnosticsDialog::OnInit(wxInitDialogEvent &event)
{
  common::DiagnosticManager::Instance()->SetEnabled(true);
}
