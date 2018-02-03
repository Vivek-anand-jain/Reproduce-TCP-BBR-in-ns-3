/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#pragma once

#include "ns3/internet-config.h"
#include <sqlite3.h>
#include <string>
#ifdef HAVE_SEMAPHORE_H
#include <semaphore.h>
#else
#error "Can't compile this unit without <semaphore.h>"
#endif

namespace ns3 {

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief A C++ interface towards an SQLITE database
 *
 * The class is able to execute commands, and retrieve results, from an SQLITE
 * database. The methods with the "Spin" prefix, in case of concurrent access
 * to the database, will spin until the operation is applied. The methods with
 * the "Wait" prefix will do operations in an atomic manner, since the process
 * will wait on a system semaphore.
 */
class SQLiteOutput
{
public:
  /**
   * \brief SQLiteOutput constructor
   * \param name database name
   * \param semName system semaphore name
   */
  SQLiteOutput (const std::string &name, const std::string &semName);
  /**
   * Destructor
   */
  ~SQLiteOutput ();

  /**
   * \brief Execute a command until the return value is OK or an ERROR
   *
   * Ignore errors due to concurrency, the method will repeat the command instead
   *
   * \param cmd Command
   * \return true in case of success
   */
  bool SpinExec (const std::string &cmd) const
  {
    if (SpinExec (m_db, cmd) == SQLITE_OK)
      return true;
    return false;
  }
  /**
   * \brief Execute a command, waiting on a system semaphore
   * \param cmd Command to be executed
   * \return true in case of success
   */
  bool WaitExec (const std::string &cmd) const
  {
    int rc = WaitExec (m_db, cmd);
    return !CheckError(m_db, rc, cmd, nullptr, false);
  }

  /**
   * \brief Execute a command, waiting on a system semaphore
   * \param stmt Sqlite3 statement to be executed
   * \return true in case of success
   */
  bool WaitExec (sqlite3_stmt *stmt) const
  {
    if (WaitExec (m_db, stmt) == SQLITE_OK)
      return true;
    return false;
  }

  /**
   * \brief Prepare a statement, waiting on a system semaphore
   * \param stmt Sqlite statement
   * \param cmd Command to prepare inside the statement
   * \return true in case of success
   */
  bool WaitPrepare (sqlite3_stmt **stmt, const std::string &cmd) const
  {
    if (WaitPrepare (m_db, stmt, cmd) == SQLITE_OK)
      return true;
    return false;
  }

  /**
   * \brief Bind a value to a sqlite statement
   * \param stmt Sqlite statement
   * \param pos Position of the bind argument inside the statement
   * \param value Value to bind
   */
  template<typename T> bool Bind (sqlite3_stmt *stmt, int pos, const T &value) const;

  /**
   * \brief Retrieve a value from an executed statement
   * \param stmt sqlite statement
   * \param pos Column position
   */
  template<typename T> T RetrieveColumn (sqlite3_stmt *stmt, int pos) const;

  /**
   * \brief Execute a step operation on a statement until the result is ok or an error
   *
   * Ignores concurrency errors; it will retry instead of failing.
   *
   * \param stmt Statement
   * \return Sqlite error core
   */
  static int SpinStep (sqlite3_stmt *stmt);
  /**
   * \brief Finalize a statement until the result is ok or an error
   *
   * Ignores concurrency errors; it will retry instead of failing.
   *
   * \param stmt Statement
   * \return Sqlite error code
   */
  static int SpinFinalize (sqlite3_stmt *stmt);

protected:
  /**
   * \brief Execute a command, waiting on a system semaphore
   * \param db Database
   * \param cmd Command
   * \return Sqlite error code
   */
  int WaitExec (sqlite3 *db, const std::string &cmd) const;
  /**
   * \brief Execute a statement, waiting on a system semaphore
   * \param db Database
   * \param stmt Statement
   * \return Sqlite error code
   */
  int WaitExec (sqlite3 *db, sqlite3_stmt *stmt) const;
  /**
   * \brief Prepare a statement, waiting on a system semaphore
   * \param db Database
   * \param stmt Statement
   * \param cmd Command to prepare
   * \return Sqlite error code
   */
  int WaitPrepare (sqlite3 *db, sqlite3_stmt **stmt, const std::string &cmd) const;

  /**
   * \brief Execute a command ignoring concurrency problems, retrying instead
   * \param db Database
   * \param cmd Command
   * \return Sqlite error code
   */
  static int SpinExec (sqlite3 *db, const std::string &cmd);
  /**
   * \brief Preparing a command ignoring concurrency problems, retrying instead
   * \param db Database
   * \param stmt Statement
   * \param cmd Command to prepare
   * \return Sqlite error code
   */
  static int SpinPrepare (sqlite3 *db, sqlite3_stmt **stmt, const std::string &cmd);

  /**
   * \brief Fail, printing an error message from sqlite
   * \param db Database
   * \param cmd Command
   */
  [[ noreturn ]] static void Error (sqlite3 *db, const std::string &cmd);
  /**
   * \brief Check any error in the db
   * \param db Database
   * \param rc Sqlite return code
   * \param cmd Command
   * \param sem System semaphore
   * \param hardExit if true, will exit the program
   * \return true in case of error, false otherwise
   */
  static bool CheckError (sqlite3 *db, int rc, const std::string &cmd,
                          sem_t *sem, bool hardExit);

private:
  std::string m_dBname;    //!< Database name
  std::string m_semName;   //!< System semaphore name
  sqlite3 *m_db {nullptr}; //!< Database pointer
};

} // namespace ns3
