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
#include "tcp-general-test.h"
#include "sqlite-output.h"

#include "ns3/test.h"
#include "ns3/tcp-vegas.h"
#include "ns3/tcp-bic.h"
#include "ns3/tcp-hybla.h"
#include "ns3/tcp-scalable.h"
#include "ns3/tcp-yeah.h"
#include "ns3/tcp-westwood.h"
#include "ns3/tcp-ledbat.h"
#include "ns3/tcp-highspeed.h"
#include "ns3/tcp-veno.h"
#include "ns3/tcp-lp.h"
#include "ns3/tcp-illinois.h"
#include "ns3/tcp-htcp.h"

static bool writeVector = false;
static bool testEnabled = false;

namespace ns3 {

/**
 * \brief Check over values emitted by trace sources during a TCP transfer.
 */
class TcpValueTest : public TcpGeneralTest
{
public:
  /**
   * \brief Constructor.
   * \param segmentSize Segment size.
   * \param packetSize Packet size.
   * \param propDel Propagation Delay.
   * \param pktInterval Pkt Interval.
   * \param pktCount Pkt count.
   * \param mtu MTU.
   * \param typeId TypeId of the congestion control.
   * \param sackEnabled enable or disable SACK
   * \param desc Description of the test
   */
  TcpValueTest (uint32_t segmentSize,
                uint32_t packetSize,
                const Time &propDel,
                const Time &pktInterval,
                uint32_t pktCount,
                uint32_t mtu,
                const TypeId &typeId,
                bool sackEnabled,
                bool timestampEnabled,
                bool limitedTransmitEnabled,
                double m_errorRate,
                const std::string &desc);
protected:
  virtual void ConfigureEnvironment () override;
  virtual void ConfigureProperties () override;
  virtual Ptr<ErrorModel> CreateReceiverErrorModel () override;

  virtual void CongStateTrace (const TcpSocketState::TcpCongState_t oldValue,
                               const TcpSocketState::TcpCongState_t newValue) override;
  virtual void CWndTrace (uint32_t oldValue, uint32_t newValue) override;
  virtual void RttTrace (Time oldTime, Time newTime) override;
  virtual void SsThreshTrace (uint32_t oldValue, uint32_t newValue) override;
  virtual void BytesInFlightTrace (uint32_t oldValue, uint32_t newValue) override;
  virtual void RtoTrace (Time oldValue, Time newValue) override;
  virtual void NextTxSeqTrace (SequenceNumber32 oldValue, SequenceNumber32 newValue) override;
  virtual void HighestTxSeqTrace (SequenceNumber32 oldValue, SequenceNumber32 newValue) override;

  virtual void DoRun () override;
  virtual void DoTeardown () override;

private:
  /**
   * \brief Insert a value into the database
   * \param out Output class
   * \param tableName Table name
   * \param value The value to insert
   */
  template<typename T> void Insert (SQLiteOutput *out, const std::string &tableName, T value) const;
  /**
   * \brief Check if the value is the same as the value stored in the database
   *
   * Consume one row in the statement stmt
   *
   * \param out Output class
   * \param stmt Sqlite statement which contains the result of a "SELECT" command
   * \param value The value to check
   */
  template<typename T> void Check (SQLiteOutput *out, sqlite3_stmt *stmt, T value);

private:
  uint32_t m_segmentSize {1};   //!< Segment size.
  uint32_t m_packetSize {1};    //!< Packet size.
  Time m_propDel;               //!< Propagation delay of the experiment
  Time m_pktInterval;           //!< Application packet interval of the experiment
  uint32_t m_pktCount {1};      //!< Application packet count
  uint32_t m_mtu {1500};        //!< MTU
  bool m_sackEnabled {true};    //!< Sack option
  bool m_timestampEnabled {true};         //!< Timestamp option
  bool m_limitedTransmitEnabled {true};   //!< Limited transmit option
  SQLiteOutput *m_sqliteOutput {nullptr}; //!< SQLite output class
  double m_errorRate {0.0};     //!< Error rate
  std::string m_tableName {""}; //!< Prefix for the table name
  sqlite3_stmt *m_readCwnd {nullptr}; //!< Statement which contains the cwnd table
  sqlite3_stmt *m_readRtt {nullptr};  //!< Statement which contains the rtt table
  sqlite3_stmt *m_readRto {nullptr};  //!< Statement which contains the rto table
  sqlite3_stmt *m_readSsthresh {nullptr}; //!< Statement which contains the ssthresh table
  sqlite3_stmt *m_readBytes {nullptr};  //!< Statement which contains the bytes in flight table
  sqlite3_stmt *m_readNexttx {nullptr}; //!< Statement which contains the next tx sequence table
  sqlite3_stmt *m_readHightx {nullptr}; //!< Statement which contains the highest tx sequence table

};

TcpValueTest::TcpValueTest (uint32_t segmentSize,
                            uint32_t packetSize,
                            const Time &propDel,
                            const Time &pktInterval,
                            uint32_t pktCount,
                            uint32_t mtu,
                            const TypeId &typeId,
                            bool sackEnabled,
                            bool timestampEnabled,
                            bool limitedTransmitEnabled,
                            double errorRate,
                            const std::string &desc)
  : TcpGeneralTest (desc),
    m_segmentSize (segmentSize),
    m_packetSize (packetSize),
    m_propDel (propDel),
    m_pktInterval (pktInterval),
    m_pktCount (pktCount),
    m_mtu (mtu),
    m_sackEnabled (sackEnabled),
    m_timestampEnabled (timestampEnabled),
    m_limitedTransmitEnabled (limitedTransmitEnabled),
    m_errorRate (errorRate)
{
  m_congControlTypeId = typeId;
}

void
TcpValueTest::DoTeardown()
{
  if (!writeVector)
    {
      int rc = m_sqliteOutput->SpinStep(m_readCwnd);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");

      rc = m_sqliteOutput->SpinFinalize(m_readCwnd);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
      rc = m_sqliteOutput->SpinFinalize(m_readRtt);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
      rc = m_sqliteOutput->SpinFinalize(m_readRto);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
      rc = m_sqliteOutput->SpinFinalize(m_readSsthresh);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
      rc = m_sqliteOutput->SpinFinalize(m_readBytes);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
      rc = m_sqliteOutput->SpinFinalize(m_readNexttx);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
      rc = m_sqliteOutput->SpinFinalize(m_readHightx);
      NS_TEST_ASSERT_MSG_NE(rc, SQLITE_ROW, "Unread rows");
    }
  delete m_sqliteOutput;
  TcpGeneralTest::DoTeardown();
}

void
TcpValueTest::DoRun ()
{
  m_sqliteOutput = new SQLiteOutput ("tcp-value-test.db", "NS3-TCP-VALUE-TEST");
  std::stringstream ss;
  bool ret;

  ss << m_congControlTypeId.GetName().erase(0, 5) << "-" << m_segmentSize << "-"
     << m_packetSize << "-" << m_propDel.GetSeconds () << "-"
     << m_pktInterval.GetSeconds() << "-" << m_mtu << "-" << m_sackEnabled << "-"
     << m_timestampEnabled << "-" << m_limitedTransmitEnabled << "-" << m_errorRate;
  m_tableName = ss.str ();

  if (writeVector)
    {
      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_cwnd\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_cwnd\" " +
                                     "(TIME DOUBLE NOT NULL, CWND INT NOT NULL);");
      NS_ABORT_IF(ret == false);

      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_rtt\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_rtt\" " +
                                     "(TIME DOUBLE NOT NULL, RTT DOUBLE NOT NULL);");
      NS_ABORT_IF(ret == false);

      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_rto\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_rto\" " +
                                     "(TIME DOUBLE NOT NULL, RTO DOUBLE NOT NULL);");
      NS_ABORT_IF(ret == false);

      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_ssthresh\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_ssthresh\" " +
                                     "(TIME DOUBLE NOT NULL, SSTHRESH INT NOT NULL);");
      NS_ABORT_IF(ret == false);

      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_bytes\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_bytes\" " +
                                     "(TIME DOUBLE NOT NULL, BYTES INT NOT NULL);");
      NS_ABORT_IF(ret == false);

      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_nexttx\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_nexttx\" " +
                                     "(TIME DOUBLE NOT NULL, NEXTTX INT NOT NULL);");
      NS_ABORT_IF(ret == false);

      m_sqliteOutput->WaitExec("DROP TABLE IF EXISTS \"" + m_tableName + "_hightx\";");
      ret = m_sqliteOutput->WaitExec("CREATE TABLE \"" + m_tableName + "_hightx\" " +
                                     "(TIME DOUBLE NOT NULL, HIGHTX INT NOT NULL);");
      NS_ABORT_IF(ret == false);
    }
  else
    {
      ret = m_sqliteOutput->WaitPrepare(&m_readCwnd, "SELECT * FROM \"" + m_tableName + "_cwnd\";");
      NS_ABORT_IF(ret == false);
      ret = m_sqliteOutput->WaitPrepare(&m_readRtt, "SELECT * FROM \"" + m_tableName + "_rtt\";");
      NS_ABORT_IF(ret == false);
      ret = m_sqliteOutput->WaitPrepare(&m_readRto, "SELECT * FROM \"" + m_tableName + "_rto\";");
      NS_ABORT_IF(ret == false);
      ret = m_sqliteOutput->WaitPrepare(&m_readSsthresh, "SELECT * FROM \"" + m_tableName + "_ssthresh\";");
      NS_ABORT_IF(ret == false);
      ret = m_sqliteOutput->WaitPrepare(&m_readBytes, "SELECT * FROM \"" + m_tableName + "_bytes\";");
      NS_ABORT_IF(ret == false);
      ret = m_sqliteOutput->WaitPrepare(&m_readNexttx, "SELECT * FROM \"" + m_tableName + "_nexttx\";");
      NS_ABORT_IF(ret == false);
      ret = m_sqliteOutput->WaitPrepare(&m_readHightx, "SELECT * FROM \"" + m_tableName + "_hightx\";");
      NS_ABORT_IF(ret == false);
    }
  TcpGeneralTest::DoRun ();
}
void
TcpValueTest::ConfigureEnvironment ()
{
  TcpGeneralTest::ConfigureEnvironment ();
  SetPropagationDelay (m_propDel);
  SetAppPktSize (m_packetSize);
  SetAppPktCount (m_pktCount);
  SetAppPktInterval (m_pktInterval);
  SetMTU (m_mtu);
}

void
TcpValueTest::ConfigureProperties ()
{
  TcpGeneralTest::ConfigureProperties ();
  SetSegmentSize (SENDER, m_segmentSize);
  SetSegmentSize (RECEIVER, m_segmentSize);

  GetReceiverSocket()->SetAttribute("Sack", BooleanValue (m_sackEnabled));
  GetReceiverSocket()->SetAttribute("Timestamp", BooleanValue (m_timestampEnabled));
  GetReceiverSocket()->SetAttribute("LimitedTransmit", BooleanValue (m_limitedTransmitEnabled));
}

Ptr<ErrorModel>
TcpValueTest::CreateReceiverErrorModel()
{
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  Ptr<RateErrorModel> error_model = CreateObject<RateErrorModel> ();
  error_model->SetRandomVariable (uv);
  error_model->SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model->SetRate (m_errorRate);
  return error_model;
}

void TcpValueTest::CongStateTrace(const TcpSocketState::TcpCongState_t oldValue, const TcpSocketState::TcpCongState_t newValue)
{

}

template<typename T>
void
TcpValueTest::Insert (SQLiteOutput *out, const std::string &tableName, T value) const
{
  bool ret;
  sqlite3_stmt *stmt;
  ret = out->WaitPrepare(&stmt, "INSERT INTO \"" + tableName + "\" VALUES (?,?);");
  NS_ABORT_IF(ret == false);
  ret = out->Bind (stmt, 1, Simulator::Now().GetSeconds());
  NS_ABORT_IF(ret == false);
  ret = out->Bind (stmt, 2, value);
  NS_ABORT_IF(ret == false);
  ret = out->WaitExec(stmt);
  NS_ABORT_IF(ret == false);
}

template<typename T>
void
TcpValueTest::Check (SQLiteOutput *out, sqlite3_stmt *stmt, T value)
{
  int rc = out->SpinStep(stmt);
  NS_ABORT_IF(rc != SQLITE_ROW);
  double timestamp = out->RetrieveColumn<double> (stmt, 0);
  T oldValue = out->RetrieveColumn<T> (stmt, 1);

  NS_TEST_ASSERT_MSG_EQ_TOL(timestamp, Simulator::Now().GetSeconds(), 0.005,
                            "Timestamp differs for Cwnd");
  NS_TEST_ASSERT_MSG_EQ(oldValue, value,
                        "New value differs from saved");
}

void TcpValueTest::CWndTrace(uint32_t oldValue, uint32_t newValue)
{
  NS_UNUSED(oldValue);
  std::string tableName = m_tableName + "_cwnd";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newValue);
    }
  else
    {
      Check (m_sqliteOutput, m_readCwnd, newValue);
    }
}

void TcpValueTest::RttTrace(Time oldTime, Time newTime)
{
  NS_UNUSED(oldTime);
  std::string tableName = m_tableName + "_rtt";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newTime.GetSeconds ());
    }
  else
    {
      Check (m_sqliteOutput, m_readRtt, newTime.GetSeconds ());
    }
}

void TcpValueTest::SsThreshTrace(uint32_t oldValue, uint32_t newValue)
{
  NS_UNUSED(oldValue);
  std::string tableName = m_tableName + "_ssthresh";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newValue);
    }
  else
    {
      Check (m_sqliteOutput, m_readSsthresh, newValue);
    }
}

void TcpValueTest::BytesInFlightTrace(uint32_t oldValue, uint32_t newValue)
{
  NS_UNUSED(oldValue);
  std::string tableName = m_tableName + "_bytes";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newValue);
    }
  else
    {
      Check (m_sqliteOutput, m_readBytes, newValue);
    }
}

void TcpValueTest::RtoTrace(Time oldValue, Time newValue)
{
  NS_UNUSED(oldValue);
  std::string tableName = m_tableName + "_rto";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newValue.GetSeconds ());
    }
  else
    {
      Check (m_sqliteOutput, m_readRto, newValue.GetSeconds ());
    }
}

void TcpValueTest::NextTxSeqTrace(SequenceNumber32 oldValue, SequenceNumber32 newValue)
{
  NS_UNUSED(oldValue);
  std::string tableName = m_tableName + "_nexttx";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newValue.GetValue());
    }
  else
    {
      Check (m_sqliteOutput, m_readNexttx, newValue.GetValue());
    }
}

void TcpValueTest::HighestTxSeqTrace(SequenceNumber32 oldValue, SequenceNumber32 newValue)
{
  NS_UNUSED(oldValue);
  std::string tableName = m_tableName + "_hightx";

  if (writeVector)
    {
      Insert (m_sqliteOutput, tableName, newValue.GetValue());
    }
  else
    {
      Check (m_sqliteOutput, m_readHightx, newValue.GetValue());
    }
}

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TCP Values test suite
 */
class TcpValuesTest : public TestSuite
{
public:
  TcpValuesTest () : TestSuite ("tcp-values-test", UNIT)
  {
    if (!testEnabled)
      {
        return;
      }
    std::list<TypeId> types = {
                               TcpNewReno::GetTypeId (),
                               TcpVegas::GetTypeId (),
                               TcpHybla::GetTypeId (),
                               TcpScalable::GetTypeId (),
                               TcpYeah::GetTypeId (),
                               TcpWestwood::GetTypeId (),
                               TcpLedbat::GetTypeId (),
                               TcpHighSpeed::GetTypeId (),
                               TcpVeno::GetTypeId (),
                               TcpLp::GetTypeId (),
                               TcpIllinois::GetTypeId (),
                               TcpHtcp::GetTypeId ()
                              };

    std::list<uint32_t> segmentSizes = {1000};
    std::list<uint32_t> pktSizes = {1000};
    std::list<uint32_t> mtus = {1500};
    std::list<Time> propDelays = {MilliSeconds(1), MilliSeconds(25), MilliSeconds(200), MilliSeconds(1000)};
    std::list<uint32_t> pkts = {1000};
    std::list<bool> sacks = {true, false};
    std::list<bool> limitTransmits = {true, false};
    std::list<bool> timestamps = {true, false};
    std::list<double> errorRates = {0.0, 0.0000001, 0.0001, 0.01};

    for (const auto &type : types)
      {
        for (const auto &segmentSize : segmentSizes)
          {
            for (const auto &mtu : mtus)
              {
                for (const auto &propDelay : propDelays)
                  {
                    for (const auto &pkt : pkts)
                      {
                        for (const auto &sack : sacks)
                          {
                            for (const auto &limitTransmit : limitTransmits)
                              {
                                for (const auto &timestamp : timestamps)
                                  {
                                    for (const auto &errorRate : errorRates)
                                      {
                                        for (const auto &pktSize : pktSizes)
                                          {
                                            AddTestCase (new TcpValueTest (segmentSize, pktSize, propDelay,
                                                                           MilliSeconds(1), pkt, mtu, type,
                                                                           sack, timestamp, limitTransmit,
                                                                           errorRate, "value-test"));
                                          }
                                      }
                                  }
                              }
                          }
                      }
                  }
              }
          }
      }
  }
};

static TcpValuesTest g_tcpValuesTest; //!< Static variable for test initialization

} // namespace ns3
