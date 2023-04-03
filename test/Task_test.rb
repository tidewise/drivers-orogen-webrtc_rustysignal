# frozen_string_literal: true

using_task_library "webrtc_rustysignal"

describe OroGen.webrtc_rustysignal.Task do
    run_live

    before do
        # Start rustysignal. We try to allocate a port "that makes sense"
        @rusty_port = auto_port
    end

    after do
        teardown_rustysignal(@rusty_pid) if @rusty_pid
    end

    it "fails to configure if rusty is not present" do
        p = deploy_signalling_task("p")
        expect_execution.scheduler(true).to do
            fail_to_start p
        end
    end

    it "goes into exception if rusty is stopped" do
        @rusty_pid = spawn_rustysignal(@rusty_port)
        p = deploy_signalling_task("p")
        syskit_configure_and_start(p)

        expect_execution { Process.kill("INT", @rusty_pid) }
            .to { emit p.stop_event }
    end

    it "communicates one-to-one through a rustysignal server" do
        @rusty_pid = spawn_rustysignal(@rusty_port)
        p1 = deploy_signalling_task("p1")
        p2 = deploy_signalling_task("p2")
        p3 = deploy_signalling_task("p3")

        syskit_configure_and_start(p1)
        syskit_configure_and_start(p2)

        Types.webrtc_base.SignallingMessageType.keys.each do |key, _|
            msg_in = Types.webrtc_base.SignallingMessage.new(
                from: "p1",
                to: "p2",
                type: key,
                message: "something",
                m_line: 0
            )
            msg_out =
                expect_execution { syskit_write p1.signalling_in_port, msg_in }
                .to do
                    have_no_new_sample p3.signalling_out_port, at_least_during: 0.1
                    have_one_new_sample p2.signalling_out_port
                end

            assert_equal msg_in, msg_out
        end
    end

    def deploy_signalling_task(peer_id)
        task = syskit_deploy(
            OroGen.webrtc_rustysignal.Task
                  .with_arguments(peer_id: peer_id)
                  .deployed_as("rustysignal_task_#{peer_id}")
        )
        task.properties.url = "ws://localhost:#{@rusty_port}"
        task.properties.disable_tls_verification = true
        task
    end

    def spawn_rustysignal(port, timeout: 5)
        pid = spawn "rustysignal", "localhost:#{port}"
        return pid if rustysignal_alive?(port, timeout: timeout)

        teardown_rustysignal(pid)
    end

    def rustysignal_alive?(port, timeout: 5)
        deadline = Time.now + timeout
        while Time.now < deadline
            begin
                TCPSocket.new("localhost", port)
                return true
            rescue SystemCallError
                sleep 0.05
            end
        end
        false
    end

    def teardown_rustysignal(pid)
        Process.kill("KILL", pid)
        Process.waitpid(pid)
    end

    def auto_port
        server = TCPServer.new(0)
        server.local_address.ip_port
    ensure
        server&.close
    end
end

Syskit.extend_model OroGen.webrtc_rustysignal.Task do
    argument :peer_id

    def update_properties
        super

        properties.peer_id = peer_id
    end
end
