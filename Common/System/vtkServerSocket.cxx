// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkServerSocket.h"

#include "vtkClientSocket.h"
#include "vtkObjectFactory.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkServerSocket);
//------------------------------------------------------------------------------
vtkServerSocket::vtkServerSocket() = default;

//------------------------------------------------------------------------------
vtkServerSocket::~vtkServerSocket() = default;

//------------------------------------------------------------------------------
int vtkServerSocket::GetServerPort()
{
  if (!this->GetConnected())
  {
    return 0;
  }
  return this->GetPort(this->SocketDescriptor);
}

//------------------------------------------------------------------------------
int vtkServerSocket::CreateServer(int port, const std::string& bindAddr)
{
  if (this->SocketDescriptor != -1)
  {
    vtkWarningMacro("Server Socket already exists. Closing old socket.");
    this->CloseSocket(this->SocketDescriptor);
    this->SocketDescriptor = -1;
  }
  this->SocketDescriptor = this->CreateSocket();
  if (this->SocketDescriptor < 0)
  {
    return -1;
  }
  if (this->BindSocket(this->SocketDescriptor, port, bindAddr) != 0 ||
    this->Listen(this->SocketDescriptor) != 0)
  {
    // failed to bind or listen.
    this->CloseSocket(this->SocketDescriptor);
    this->SocketDescriptor = -1;
    return -1;
  }
  // Success.
  return 0;
}

//------------------------------------------------------------------------------
int vtkServerSocket::CreateServer(int port)
{
  return this->CreateServer(port, "0.0.0.0");
}

//------------------------------------------------------------------------------
vtkClientSocket* vtkServerSocket::WaitForConnection(unsigned long msec /*=0*/)
{
  if (this->SocketDescriptor < 0)
  {
    vtkErrorMacro("Server Socket not created yet!");
    return nullptr;
  }

  int ret = this->SelectSocket(this->SocketDescriptor, msec);
  if (ret == 0)
  {
    // Timed out.
    return nullptr;
  }
  if (ret == -1)
  {
    vtkErrorMacro("Error selecting socket.");
    return nullptr;
  }
  int clientsock = this->Accept(this->SocketDescriptor);
  if (clientsock == -1)
  {
    vtkErrorMacro("Failed to accept the socket.");
    return nullptr;
  }
  // Create a new vtkClientSocket and return it.
  vtkClientSocket* cs = vtkClientSocket::New();
  cs->SocketDescriptor = clientsock;
  cs->SetConnectingSide(false);
  return cs;
}

//------------------------------------------------------------------------------
void vtkServerSocket::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
